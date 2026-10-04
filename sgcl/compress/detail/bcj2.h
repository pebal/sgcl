//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "range_coder.h"
#include "../error.h"
#include "../../core/detail/bytes.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace sgcl::compress::detail {
    // BCJ2, 7-Zip's x86 branch converter over four streams: the main one
    // (the code, every E8 call, E9 jump and 0F 8x conditional jump opcode
    // in it, the operands of the converted ones taken out), the absolute
    // targets of the converted calls and of the converted jumps (32 bits
    // big-endian each), and a range-coded stream of one bit per opcode —
    // converted or not — coded with a probability per byte before an E8,
    // one for E9 and one for the conditional jumps (the probabilities and
    // the coder of LZMA). An opcode that is the last byte of the data has
    // no bit; one with fewer than four bytes after it is never converted.
    struct Bcj2Streams {
        std::vector<uint8_t> main;
        std::vector<uint8_t> call;
        std::vector<uint8_t> jump;
        std::vector<uint8_t> rc;
    };

    namespace bcj2 {
        SGCL_LZMA_INLINE bool is_branch(uint8_t prev, uint8_t b) noexcept {
            return (b & 0xFE) == 0xE8 || (prev == 0x0F && (b & 0xF0) == 0x80);
        }

        SGCL_LZMA_INLINE size_t prob_index(uint8_t prev, uint8_t b) noexcept {
            return b == 0xE8 ? 2 + size_t(prev) : b == 0xE9 ? 1 : 0;
        }

        constexpr size_t Probs = 2 + 256;
    }

    // The data split into the four streams. A branch is converted when its
    // operand is a near relative target (its top byte 00 or FF), as
    // compilers make calls within a program.
    inline Bcj2Streams bcj2_encode(const uint8_t* p, size_t n) noexcept {
        Bcj2Streams s;
        s.main.reserve(n);
        RangeEncoder rc(s.rc);
        uint16_t probs[bcj2::Probs];
        std::fill(std::begin(probs), std::end(probs), rc::ProbInit);
        uint8_t prev = 0;
        size_t i = 0;
        while (i < n) {
            uint8_t b = p[i++];
            s.main.push_back(b);
            if (!bcj2::is_branch(prev, b)) {
                prev = b;
                continue;
            }
            if (i == n) {
                break;
            }
            uint16_t& prob = probs[bcj2::prob_index(prev, b)];
            if (i + 4 <= n && (p[i + 3] == 0 || p[i + 3] == 0xFF)) {
                uint32_t src;
                std::memcpy(&src, p + i, 4);
                uint32_t dest = src + uint32_t(i + 4);
                auto& out = b == 0xE8 ? s.call : s.jump;
                uint8_t be[4] = {uint8_t(dest >> 24), uint8_t(dest >> 16), uint8_t(dest >> 8), uint8_t(dest)};
                out.insert(out.end(), be, be + 4);
                rc.bit(prob, 1);
                prev = p[i + 3];
                i += 4;
            } else {
                rc.bit(prob, 0);
                prev = b;
            }
        }
        rc.finish();
        return s;
    }

    // The data put back together, out_size bytes of it, a piece at a time.
    // The four streams are fed as they come: decode() stops where a stream
    // it needs has nothing left and need() names it; feed() gives that
    // stream's next bytes (the bytes not yet taken first: an operand's four
    // bytes are taken together). A stream fed as ended that runs dry is
    // errc::unexpected_end. The range coder normalises before a bit rather
    // than after it (the same bits either way), so that a wait for its
    // stream never falls inside one.
    class Bcj2Decoder {
    public:
        enum : int { Main = 0, Call = 1, Jump = 2, Rc = 3 };

        errc error = errc::corrupt;
        const char* error_text = nullptr;

        void init(uint64_t out_size) noexcept {
            for (auto& s : _in) {
                s = Input();
            }
            _size = out_size;
            _out = 0;
            _prev = 0;
            _held = 0;
            _failed = false;
            _started = false;
            _state = State::plain;
            _need = -1;
            std::fill(std::begin(_probs), std::end(_probs), rc::ProbInit);
            _range = 0xFFFFFFFF;
            _code = 0;
        }

        // The four streams whole, in memory
        SGCL_INLINE_HOT void init(const uint8_t* main, size_t main_size, const uint8_t* call, size_t call_size, const uint8_t* jump, size_t jump_size,
                  const uint8_t* rc, size_t rc_size, uint64_t out_size) noexcept {
            init(out_size);
            feed(Main, main, main_size, true);
            feed(Call, call, call_size, true);
            feed(Jump, jump, jump_size, true);
            feed(Rc, rc, rc_size, true);
        }

        // The bytes of stream k at hand from now on
        SGCL_INLINE_HOT void feed(int k, const uint8_t* p, size_t n, bool ended) noexcept {
            _in[k] = {p, p + n, ended};
        }

        // Of the bytes fed to stream k, those not yet taken
        SGCL_INLINE_HOT size_t left(int k) const noexcept {
            return size_t(_in[k].end - _in[k].at);
        }

        // The stream the last decode() stopped for, or -1
        SGCL_INLINE_HOT int need() const noexcept {
            return _need;
        }

        SGCL_INLINE_HOT bool failed() const noexcept {
            return _failed;
        }

        SGCL_INLINE_HOT bool done() const noexcept {
            return !_failed && _out == _size && _held == 0 && _state == State::plain;
        }

        // After the end: the range coder at zero (its last normalisation
        // taken from what is left), as the encoder's flush leaves it
        bool finished_ok() noexcept {
            Input& r = _in[Rc];
            while (_range < rc::Top && r.at < r.end) {
                _range <<= 8;
                _code = (_code << 8) | *r.at++;
            }
            return _started && _code == 0;
        }

        // Up to n bytes into out; the count made
        size_t decode(uint8_t* out, size_t n) noexcept {
            _need = -1;
            size_t made = 0;
            if (!_started) {
                Input& r = _in[Rc];
                if (left(Rc) < 5) {
                    _starve(Rc);
                    return 0;
                }
                if (r.at[0] != 0) {
                    _fail(errc::corrupt, "bcj2: the range coder's first byte is not 0");
                    return 0;
                }
                _code = uint32_t(r.at[1]) << 24 | uint32_t(r.at[2]) << 16 | uint32_t(r.at[3]) << 8 | r.at[4];
                r.at += 5;
                _started = true;
            }
            while (made < n && !_failed) {
                if (_held) {
                    size_t k = std::min<size_t>(_held, n - made);
                    sgcl::detail::copy_bytes(out + made, _hold + (4 - _held), k);
                    made += k;
                    _held -= uint32_t(k);
                    continue;
                }
                if (_state == State::operand) {
                    Input& s = _in[_stream];
                    if (left(_stream) < 4) {
                        _starve(_stream);
                        break;
                    }
                    uint32_t dest = uint32_t(s.at[0]) << 24 | uint32_t(s.at[1]) << 16 | uint32_t(s.at[2]) << 8 | s.at[3];
                    s.at += 4;
                    uint32_t rel = dest - uint32_t(_out + 4);
                    std::memcpy(_hold, &rel, 4);
                    _held = 4;
                    _out += 4;
                    _prev = uint8_t(rel >> 24);
                    _state = State::plain;
                    continue;
                }
                if (_state == State::bit) {
                    if (!_normalize()) {
                        break;
                    }
                    uint16_t& prob = _probs[_prob];
                    uint32_t bound = (_range >> rc::ProbBits) * prob;
                    if (_code < bound) {
                        _range = bound;
                        prob = uint16_t(prob + ((rc::ProbOne - prob) >> rc::MoveBits));
                        _prev = _branch;
                        _state = State::plain;
                    } else {
                        _range -= bound;
                        _code -= bound;
                        prob = uint16_t(prob - (prob >> rc::MoveBits));
                        if (_size - _out < 4) {
                            _fail(errc::corrupt, "bcj2: a converted branch past the end of the data");
                            break;
                        }
                        _stream = _branch == 0xE8 ? Call : Jump;
                        _state = State::operand;
                    }
                    continue;
                }
                if (_out == _size) {
                    break;
                }
                // the plain bytes up to the next branch opcode
                Input& m = _in[Main];
                size_t avail = left(Main);
                if (avail == 0) {
                    _starve(Main);
                    break;
                }
                size_t k = std::min<size_t>({n - made, size_t(std::min<uint64_t>(_size - _out, avail))});
                uint8_t prev = _prev;
                size_t j = 0;
                bool branch = false;
                while (j < k) {
                    uint8_t b = m.at[j++];
                    out[made + j - 1] = b;
                    if (bcj2::is_branch(prev, b)) {
                        branch = true;
                        break;
                    }
                    prev = b;
                }
                m.at += j;
                made += j;
                _out += j;
                if (!branch) {
                    _prev = prev;
                    continue;
                }
                uint8_t b = m.at[-1];
                if (_out == _size) {
                    _prev = b;   // the last byte: no bit after it
                    break;
                }
                _branch = b;
                _prob = bcj2::prob_index(prev, b);
                _state = State::bit;
            }
            return made;
        }

    private:
        struct Input {
            const uint8_t* at = nullptr;
            const uint8_t* end = nullptr;
            bool ended = false;
        };

        enum class State : uint8_t {
            plain,     // copying the main stream
            bit,       // an opcode's bit to decode
            operand    // a converted operand to read
        };

        bool _normalize() noexcept {
            while (_range < rc::Top) {
                Input& r = _in[Rc];
                if (r.at == r.end) {
                    _starve(Rc);
                    return false;
                }
                _range <<= 8;
                _code = (_code << 8) | *r.at++;
            }
            return true;
        }

        void _starve(int k) noexcept {
            if (_in[k].ended) {
                static constexpr const char* texts[4] = {"bcj2: the main stream ends before the data", "bcj2: the call stream ends early",
                                                         "bcj2: the jump stream ends early", "bcj2: the range coder's stream ends early"};
                _fail(errc::unexpected_end, texts[k]);
            } else {
                _need = k;
            }
        }

        void _fail(errc code, const char* text) noexcept {
            _failed = true;
            error = code;
            error_text = text;
        }

        Input _in[4];
        uint16_t _probs[bcj2::Probs];
        uint32_t _range = 0xFFFFFFFF;
        uint32_t _code = 0;
        uint64_t _size = 0;
        uint64_t _out = 0;       // the bytes of the data made, those held included
        uint8_t _prev = 0;
        uint8_t _branch = 0;     // the opcode whose bit is next
        size_t _prob = 0;
        int _stream = Call;      // the stream of the operand to read
        uint8_t _hold[4];        // a converted operand not yet handed out
        uint32_t _held = 0;
        State _state = State::plain;
        int _need = -1;
        bool _started = false;
        bool _failed = false;
    };
}
