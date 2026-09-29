//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bcj.h"
#include "bcj2.h"
#include "lzma2.h"
#include "ppmd7.h"
#include "sevenzip_aes.h"
#include "sevenzip_header.h"
#include "source.h"
#include "bzip2.h"
#include "inflate.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace sgcl::compress::detail {
    // A folder of a 7z archive decoded: its coders as a tree of pulls from
    // the output down — each coder reads what it needs from the coders or
    // the packed streams under it, as far as its output's size in the
    // header — so that the folder's output comes a piece at a time, in
    // plain memory the decoder owns. A decoder is made per use (a reader of
    // one entry decodes its folder from the start); the memory its coders
    // ask for is held against the limit before any is taken.
    namespace sevenzip_folder {
        using namespace sevenzip_format;

        // A failure as the decoder keeps it: plain data (the decoder lives
        // in plain memory), made a compress::error where it leaves
        struct Failure {
            errc code = errc::corrupt;
            uint64_t at = 0;
            std::string text;
            bool io = false;            // the source failed: its error's parts
            error_code io_code;
            std::string io_op;
            std::string io_path;

            error to_error() const {
                if (io) {
                    return error(io::error(io_code, string(io_op), string(io_path)), at);
                }
                return error(code, at, string(text));
            }
        };

        struct Context {
            const Source* source = nullptr;
            sevenzip_aes::Keys* keys = nullptr;   // the password's keys, when one was given
            std::optional<Failure> failure;
            uint64_t origin = 0;   // the folder's first packed byte, for the messages

            bool fail(errc code, const std::string& text) {
                if (!failure) {
                    failure = Failure{code, origin, text, false, {}, {}, {}};
                }
                return false;
            }

            bool fail(const io::error& e, uint64_t at) {
                if (!failure) {
                    Failure f;
                    f.code = errc::io;
                    f.at = at;
                    f.io = true;
                    f.io_code = e.code();
                    f.io_op.assign(e.op().data(), e.op().size());
                    f.io_path.assign(e.path().data(), e.path().size());
                    failure = std::move(f);
                }
                return false;
            }
        };

        // A coder's output, pulled; never past its size
        class Node {
        public:
            Node(Context& c, uint64_t size) noexcept
            : _ctx(c), _size(size) {
            }

            virtual ~Node() = default;

            // Up to n bytes into out, got 0 at the end of the output;
            // false when the folder failed (the context holds the error)
            bool read(uint8_t* out, size_t n, size_t& got) {
                got = 0;
                if (_ctx.failure) {
                    return false;
                }
                size_t k = size_t(std::min<uint64_t>(n, _size - _done));
                if (k == 0) {
                    // an output of no bytes has its end checked at its first read
                    if (!_checked) {
                        _checked = true;
                        return _finish();
                    }
                    return true;
                }
                if (!_produce(out, k, got)) {
                    return false;
                }
                if (got == 0) {
                    return _ctx.fail(errc::corrupt, "7z: a coder's output ends before its size in the header");
                }
                _done += got;
                if (_done == _size) {
                    _checked = true;
                    if (!_finish()) {
                        return false;
                    }
                }
                return true;
            }

            // Whether the output was read to its size
            bool complete() const noexcept {
                return _done == _size;
            }

        protected:
            // got 0: the coder ended
            virtual bool _produce(uint8_t* out, size_t n, size_t& got) = 0;

            // At the output's size: that the coder's data ends there too
            // (a damaged stream that decodes to the right bytes and then
            // goes on is caught here, as 7-Zip catches it)
            virtual bool _finish() {
                return true;
            }

            bool _past_size() {
                return _ctx.fail(errc::corrupt, "7z: a coder's data goes on past its size in the header");
            }

            Context& _ctx;
            uint64_t _size;
            uint64_t _done = 0;
            bool _checked = false;
        };

        // The input of a coder from the node under it, a block at a time;
        // the bytes not yet taken kept at the front when more come
        struct Feed {
            static constexpr size_t Capacity = size_t(64) << 10;

            Node* node = nullptr;
            std::vector<uint8_t> buffer;
            size_t begin = 0;
            size_t end = 0;
            bool ended = false;

            const uint8_t* data() const noexcept {
                return buffer.data() + begin;
            }

            size_t size() const noexcept {
                return end - begin;
            }

            bool fill() {
                if (buffer.size() < Capacity) {
                    buffer.resize(Capacity);
                }
                if (begin) {
                    std::memmove(buffer.data(), buffer.data() + begin, end - begin);
                    end -= begin;
                    begin = 0;
                }
                size_t got;
                if (!node->read(buffer.data() + end, Capacity - end, got)) {
                    return false;
                }
                end += got;
                ended = got == 0;
                return true;
            }
        };

        class PackNode final : public Node {
        public:
            PackNode(Context& c, uint64_t offset, uint64_t size) noexcept
            : Node(c, size), _offset(offset) {
            }

        private:
            bool _produce(uint8_t* out, size_t n, size_t& got) override {
                auto r = _ctx.source->read_at(out, n, _offset + _done);
                if (!r) {
                    return _ctx.fail(r.error(), _offset + _done);
                }
                got = *r;
                return true;
            }

            uint64_t _offset;
        };

        class CopyNode final : public Node {
        public:
            CopyNode(Context& c, uint64_t size, Node* in) noexcept
            : Node(c, size), _in(in) {
            }

        private:
            bool _produce(uint8_t* out, size_t n, size_t& got) override {
                return _in->read(out, n, got);
            }

            Node* _in;
        };

        // LZMA and LZMA2: a window of the dictionary (no larger than the
        // output), the output handed out from it
        template<class Decoder>
        class WindowNode final : public Node {
        public:
            WindowNode(Context& c, uint64_t size, Node* in, size_t window) noexcept
            : Node(c, size), _window_size(std::max<size_t>(window, 1)) {
                _feed.node = in;
            }

            Decoder decoder;

        private:
            bool _produce(uint8_t* out, size_t n, size_t& got) override {
                if (!_window) {
                    _window.reset(new uint8_t[_window_size]);
                }
                for (;;) {
                    if (_from < _pos) {
                        got = std::min(n, _pos - _from);
                        std::memcpy(out, _window.get() + _from, got);
                        _from += got;
                        return true;
                    }
                    if (_ended) {
                        got = 0;
                        return true;
                    }
                    if (_pos == _window_size) {
                        _pos = _from = 0;
                    }
                    const uint8_t* in = _feed.data();
                    size_t limit = std::min(_window_size, _pos + std::max<size_t>(n, size_t(64) << 10));
                    auto st = decoder.template decode<false>(_window.get(), _window_size, _pos, limit, in, in + _feed.size(), _feed.ended);
                    _feed.begin += size_t(in - _feed.data());
                    switch (st) {
                        case LzmaStatus::done:
                            _ended = true;
                            break;
                        case LzmaStatus::failed:
                            return _ctx.fail(decoder.error, decoder.error_text ? decoder.error_text : "7z: corrupt data");
                        case LzmaStatus::need_room:
                            break;
                        case LzmaStatus::need_input:
                            if (_from == _pos && !_feed.fill()) {
                                return false;
                            }
                            break;
                    }
                }
            }

            bool _finish() override {
                while (!_ended) {
                    if (_pos == _window_size) {
                        _pos = _from = 0;
                    }
                    const uint8_t* in = _feed.data();
                    auto st = decoder.template decode<false>(_window.get(), _window_size, _pos, _pos, in, in + _feed.size(), _feed.ended);
                    _feed.begin += size_t(in - _feed.data());
                    if (st == LzmaStatus::done) {
                        _ended = true;
                    } else if (st == LzmaStatus::failed) {
                        return _ctx.fail(decoder.error, decoder.error_text ? decoder.error_text : "7z: corrupt data");
                    } else if (st == LzmaStatus::need_room) {
                        return _past_size();
                    } else if (!_feed.fill()) {
                        return false;
                    }
                }
                return true;
            }

            Feed _feed;
            std::unique_ptr<uint8_t[]> _window;
            size_t _window_size;
            size_t _pos = 0;
            size_t _from = 0;
            bool _ended = false;
        };

        class PpmdNode final : public Node {
        public:
            PpmdNode(Context& c, uint64_t size, Node* in, uint32_t order, uint32_t memory)
            : Node(c, size) {
                _feed.node = in;
                _decoder = std::make_unique<Ppmd7Decoder>();
                _decoder->reset(order, memory, size);
            }

        private:
            bool _produce(uint8_t* out, size_t n, size_t& got) override {
                size_t pos = 0;
                for (;;) {
                    const uint8_t* in = _feed.data();
                    auto st = _decoder->decode(out, pos, n, in, in + _feed.size(), _feed.ended);
                    _feed.begin += size_t(in - _feed.data());
                    if (st == LzmaStatus::failed) {
                        return _ctx.fail(_decoder->error, _decoder->error_text ? _decoder->error_text : "7z: corrupt data");
                    }
                    if (pos || st == LzmaStatus::done) {
                        got = pos;
                        return true;
                    }
                    if (st == LzmaStatus::need_input && !_feed.fill()) {
                        return false;
                    }
                }
            }

            bool _finish() override {
                return _decoder->finished_ok() || _ctx.fail(errc::corrupt, "7z: the PPMd data does not end at its size");
            }

            Feed _feed;
            std::unique_ptr<Ppmd7Decoder> _decoder;
        };

        // BCJ and Delta over the node under them
        class FilterNode final : public Node {
        public:
            FilterNode(Context& c, uint64_t size, Node* in, const SimpleFilter& f)
            : Node(c, size), _in(in) {
                _chain.add(f);
            }

        private:
            bool _produce(uint8_t* out, size_t n, size_t& got) override {
                for (;;) {
                    if (size_t r = _chain.ready_size()) {
                        got = std::min(n, r);
                        std::memcpy(out, _chain.ready(), got);
                        _chain.take(got);
                        return true;
                    }
                    if (_flushed) {
                        got = 0;
                        return true;
                    }
                    size_t room = _chain.room();
                    size_t g;
                    if (!_in->read(_chain.tail(), room, g)) {
                        return false;
                    }
                    if (g) {
                        _chain.pushed(g);
                        _chain.run(false);
                    } else {
                        _chain.run(true);
                        _flushed = true;
                    }
                }
            }

            Node* _in;
            FilterChain _chain;
            bool _flushed = false;
        };

        class Bcj2Node final : public Node {
        public:
            Bcj2Node(Context& c, uint64_t size, Node* const* in)
            : Node(c, size) {
                for (int k = 0; k < 4; ++k) {
                    _feed[k].node = in[k];
                }
                _decoder.init(size);
            }

        private:
            bool _produce(uint8_t* out, size_t n, size_t& got) override {
                for (;;) {
                    got = _decoder.decode(out, n);
                    if (got) {
                        return true;
                    }
                    if (_decoder.failed()) {
                        return _ctx.fail(_decoder.error, _decoder.error_text);
                    }
                    int k = _decoder.need();
                    if (k < 0) {
                        return true;   // the end
                    }
                    Feed& f = _feed[k];
                    f.begin = f.end - _decoder.left(k);
                    if (!f.fill()) {
                        return false;
                    }
                    _decoder.feed(k, f.data(), f.size(), f.ended);
                }
            }

            // Every stream used up: what the decoder has not taken of it, and
            // what its coder has not given (a probe of one byte, which also
            // checks the end of a stream it never read)
            bool _finish() override {
                if (!_decoder.finished_ok()) {
                    return _ctx.fail(errc::corrupt, "7z: the BCJ2 range coder does not end at zero");
                }
                for (int k = 0; k < 4; ++k) {
                    Feed& f = _feed[k];
                    size_t untaken = f.buffer.empty() ? 0 : _decoder.left(k);
                    uint8_t probe[1];
                    size_t got = 0;
                    if (untaken || !f.node->read(probe, 1, got)) {
                        return untaken ? _past_size() : false;
                    }
                    if (got) {
                        return _past_size();
                    }
                }
                return true;
            }

            Feed _feed[4];
            Bcj2Decoder _decoder;
        };

        // 7zAES: AES-256-CBC over whole blocks of the input; the output as
        // long as the header says (the padding of the last block dropped)
        class AesNode final : public Node {
        public:
            AesNode(Context& c, uint64_t size, Node* in, const crypto::secret<32>& key, const uint8_t* iv)
            : Node(c, size), _cbc(key, iv), _plain(Feed::Capacity) {
                _feed.node = in;
            }

        private:
            bool _produce(uint8_t* out, size_t n, size_t& got) override {
                for (;;) {
                    if (_from < _to) {
                        got = std::min(n, _to - _from);
                        std::memcpy(out, _plain.data() + _from, got);
                        _from += got;
                        return true;
                    }
                    size_t whole = _feed.size() & ~size_t(15);
                    if (whole == 0) {
                        if (_feed.ended) {
                            if (_feed.size()) {
                                return _ctx.fail(errc::corrupt, "7z: encrypted data not in whole blocks");
                            }
                            got = 0;
                            return true;
                        }
                        if (!_feed.fill()) {
                            return false;
                        }
                        continue;
                    }
                    std::memcpy(_plain.data(), _feed.data(), whole);
                    _feed.begin += whole;
                    _cbc.decrypt(_plain.data(), whole);
                    _from = 0;
                    _to = whole;
                }
            }

            Feed _feed;
            sevenzip_aes::Cbc _cbc;
            std::vector<uint8_t> _plain;
            size_t _from = 0;
            size_t _to = 0;
        };

        // Deflate and Deflate64: a window of twice the history the matches
        // reach (32 KB, Deflate64's 64 KB), the output handed out from it
        class DeflateNode final : public Node {
        public:
            DeflateNode(Context& c, uint64_t size, Node* in, bool wide)
            : Node(c, size)
            , _state(std::make_unique<InflateState>())
            , _history(wide ? Window64Size : WindowSize)
            , _window(2 * _history + MaxMatch + 8)
            , _wide(wide) {
                _feed.node = in;
                _state->reset();
            }

        private:
            InflateStatus _inflate(const uint8_t*& in, size_t capacity) noexcept {
                const uint8_t* end = in + _feed.size();
                return _wide ? inflate64(*_state, in, end, _window.data(), _pos, capacity) : inflate(*_state, in, end, _window.data(), _pos, capacity);
            }

            void _slide() noexcept {
                // everything handed out: the last of the history stays
                size_t keep = std::min<size_t>(_pos, _history);
                std::memmove(_window.data(), _window.data() + _pos - keep, keep);
                _pos = _from = keep;
            }

            bool _produce(uint8_t* out, size_t n, size_t& got) override {
                for (;;) {
                    if (_from < _pos) {
                        got = std::min(n, _pos - _from);
                        std::memcpy(out, _window.data() + _from, got);
                        _from += got;
                        return true;
                    }
                    if (_ended) {
                        got = 0;
                        return true;
                    }
                    if (_pos + MaxMatch + 8 > 2 * _history) {
                        _slide();
                    }
                    const uint8_t* in = _feed.data();
                    size_t before = _pos;
                    auto st = _inflate(in, 2 * _history);
                    _feed.begin += size_t(in - _feed.data());
                    if (st == InflateStatus::failed) {
                        return _ctx.fail(_state->error, _state->error_text ? _state->error_text : (_wide ? "7z: corrupt Deflate64 data" : "7z: corrupt Deflate data"));
                    }
                    if (st == InflateStatus::done) {
                        _ended = true;
                    } else if (st == InflateStatus::need_input && _pos == before) {
                        if (_feed.ended) {
                            return _ctx.fail(errc::unexpected_end, (_wide ? "7z: the Deflate64 data ends early" : "7z: the Deflate data ends early"));
                        }
                        if (!_feed.fill()) {
                            return false;
                        }
                    }
                }
            }

            bool _finish() override {
                while (!_ended) {
                    if (_pos + MaxMatch + 8 > 2 * _history) {
                        _slide();
                    }
                    const uint8_t* in = _feed.data();
                    size_t before = _pos;
                    auto st = _inflate(in, _pos);
                    _feed.begin += size_t(in - _feed.data());
                    if (st == InflateStatus::failed) {
                        return _ctx.fail(_state->error, _state->error_text ? _state->error_text : (_wide ? "7z: corrupt Deflate64 data" : "7z: corrupt Deflate data"));
                    }
                    if (st == InflateStatus::done) {
                        _ended = true;
                    } else if (st == InflateStatus::need_room || _pos != before) {
                        return _past_size();
                    } else if (_feed.ended) {
                        return _ctx.fail(errc::unexpected_end, (_wide ? "7z: the Deflate64 data ends early" : "7z: the Deflate data ends early"));
                    } else if (!_feed.fill()) {
                        return false;
                    }
                }
                return true;
            }

            Feed _feed;
            std::unique_ptr<InflateState> _state;
            size_t _history;
            std::vector<uint8_t> _window;
            bool _wide;
            size_t _pos = 0;
            size_t _from = 0;
            bool _ended = false;
        };

        // BZip2: the decoder writes into the output directly (a block comes
        // out only when the whole of it has been read)
        class Bzip2Node final : public Node {
        public:
            Bzip2Node(Context& c, uint64_t size, Node* in)
            : Node(c, size)
            , _decoder(std::make_unique<Bzip2Decoder>()) {
                _feed.node = in;
                _decoder->reset();
            }

        private:
            bool _produce(uint8_t* out, size_t n, size_t& got) override {
                size_t pos = 0;
                for (;;) {
                    const uint8_t* in = _feed.data();
                    auto st = _decoder->decode(in, in + _feed.size(), _feed.ended, out, pos, n);
                    _feed.begin += size_t(in - _feed.data());
                    if (st == Bzip2Status::failed) {
                        return _ctx.fail(_decoder->error, _decoder->error_text ? _decoder->error_text : "7z: corrupt BZip2 data");
                    }
                    if (pos || st == Bzip2Status::done) {
                        got = pos;
                        return true;
                    }
                    if (st == Bzip2Status::need_input && !_feed.fill()) {
                        return false;
                    }
                }
            }

            bool _finish() override {
                uint8_t none[1];
                for (;;) {
                    size_t pos = 0;
                    const uint8_t* in = _feed.data();
                    auto st = _decoder->decode(in, in + _feed.size(), _feed.ended, none, pos, 0);
                    _feed.begin += size_t(in - _feed.data());
                    if (st == Bzip2Status::done) {
                        return true;
                    }
                    if (st == Bzip2Status::failed) {
                        return _ctx.fail(_decoder->error, _decoder->error_text ? _decoder->error_text : "7z: corrupt BZip2 data");
                    }
                    if (st == Bzip2Status::need_room) {
                        return _past_size();
                    }
                    if (!_feed.fill()) {
                        return false;
                    }
                }
            }

            Feed _feed;
            std::unique_ptr<Bzip2Decoder> _decoder;
        };

        inline const char* method_name(uint64_t m) {
            switch (m) {
                case Copy: return "Copy";
                case Delta: return "Delta";
                case X86: return "BCJ";
                case Bcj2: return "BCJ2";
                case PowerPC: return "PPC";
                case Ia64: return "IA64";
                case Arm: return "ARM";
                case ArmThumb: return "ARMT";
                case Sparc: return "SPARC";
                case Arm64: return "ARM64";
                case Riscv: return "RISCV";
                case Lzma: return "LZMA";
                case Lzma2: return "LZMA2";
                case Ppmd: return "PPMd";
                case Deflate: return "Deflate";
                case Deflate64: return "Deflate64";
                case Bzip2: return "BZip2";
                case Aes: return "7zAES";
                default: return nullptr;
            }
        }

        inline bool simple(uint64_t m, SimpleKind& k) noexcept {
            switch (m) {
                case X86: k = SimpleKind::x86; return true;
                case PowerPC: k = SimpleKind::powerpc; return true;
                case Ia64: k = SimpleKind::ia64; return true;
                case Arm: k = SimpleKind::arm; return true;
                case ArmThumb: k = SimpleKind::armt; return true;
                case Sparc: k = SimpleKind::sparc; return true;
                case Arm64: k = SimpleKind::arm64; return true;
                case Riscv: k = SimpleKind::riscv; return true;
                case Delta: k = SimpleKind::delta; return true;
                default: return false;
            }
        }

        // What a coder's decoder takes, from its properties and its output's
        // size; false for a method the library does not decode (the text says which)
        inline bool memory(const Coder& c, uint64_t size, uint64_t& bytes, std::string& why) {
            switch (c.method) {
                case Copy:
                    bytes = 0;
                    return true;
                case Lzma: {
                    LzmaProperties p;
                    if (c.props.size() < 5 || !LzmaProperties::from_byte(c.props[0], p)) {
                        why = "7z: invalid LZMA properties";
                        return false;
                    }
                    uint64_t d = std::max(le32(c.props.data() + 1), lzma_model::DictionaryMin);
                    bytes = std::min(d, size) + uint64_t(p.literal_probs()) * 2 + Feed::Capacity;
                    return true;
                }
                case Lzma2: {
                    uint32_t d;
                    if (c.props.size() < 1 || !lzma2_dictionary(c.props[0], d)) {
                        why = "7z: invalid LZMA2 properties";
                        return false;
                    }
                    bytes = std::min<uint64_t>(std::max(d, lzma_model::DictionaryMin), size) + (uint64_t(0x300) << 4) * 2 + Feed::Capacity;
                    return true;
                }
                case Ppmd: {
                    if (c.props.size() < 5) {
                        why = "7z: invalid PPMd properties";
                        return false;
                    }
                    uint32_t order = c.props[0];
                    uint32_t mem = le32(c.props.data() + 1);
                    if (order < ppmd::MinOrder || order > ppmd::MaxOrder || mem < ppmd::MinMemory || mem > ppmd::MaxMemory) {
                        why = "7z: invalid PPMd properties";
                        return false;
                    }
                    bytes = uint64_t(mem) + sizeof(Ppmd7) + Feed::Capacity;
                    return true;
                }
                case Bzip2:
                    bytes = uint64_t(8) << 20;
                    return true;
                case Deflate:
                    bytes = uint64_t(256) << 10;
                    return true;
                case Deflate64:
                    bytes = uint64_t(384) << 10;
                    return true;
                case Bcj2:
                    bytes = 4 * Feed::Capacity;
                    return true;
                case Aes:
                    bytes = 2 * Feed::Capacity;
                    return true;
                default: {
                    SimpleKind k;
                    if (simple(c.method, k)) {
                        bytes = FilterChain::Capacity;
                        return true;
                    }
                    const char* name = method_name(c.method);
                    if (c.method == Aes) {
                        why = "7z: encrypted data (7zAES) is not supported";
                    } else if (name) {
                        why = std::string("7z: method ") + name + " is not supported";
                    } else {
                        char hex[20];
                        std::snprintf(hex, sizeof(hex), "%llX", static_cast<unsigned long long>(c.method));
                        why = std::string("7z: unknown method ") + hex;
                    }
                    return false;
                }
            }
        }

        class Decoder {
        public:
            // The folder's decoder, or its error
            Decoder(const Source& source, const Streams& streams, size_t folder, const limits& l, sevenzip_aes::Keys* keys = nullptr)
            : _folder(streams.folders[folder]) {
                _ctx.source = &source;
                _ctx.keys = keys;
                const Folder& f = _folder;
                _ctx.origin = f.packed.empty() || f.first_pack >= streams.pack_offsets.size() ? 0 : streams.pack_offsets[f.first_pack];
                uint64_t total = 0;
                for (size_t i = 0; i < f.coders.size(); ++i) {
                    uint64_t bytes;
                    std::string why;
                    if (!memory(f.coders[i], f.sizes[i], bytes, why)) {
                        _ctx.fail(why.find("invalid") != std::string::npos ? errc::corrupt : errc::unsupported, why);
                        return;
                    }
                    total += bytes;
                }
                for (auto& c : f.coders) {
                    if (c.method != Aes) {
                        continue;
                    }
                    sevenzip_aes::Props p;
                    if (!sevenzip_aes::parse(c.props, p)) {
                        _ctx.fail(errc::corrupt, "7z: invalid 7zAES properties");
                        return;
                    }
                    if (p.k > sevenzip_aes::MaxRounds && p.k != sevenzip_aes::NoHash) {
                        _ctx.fail(errc::too_large, "7z: a key derivation of 2^" + std::to_string(p.k) + " rounds, past the limit of 2^24");
                        return;
                    }
                    if (!keys || !keys->has_password()) {
                        _ctx.fail(errc::password_required, "7z: password required");
                        return;
                    }
                }
                if (total > l.max_memory) {
                    _ctx.fail(errc::too_large, "7z: the folder's decoders need more memory than the limit allows");
                    return;
                }
                for (size_t p = 0; p < f.packed.size(); ++p) {
                    size_t k = f.first_pack + p;
                    if (streams.pack_offsets[k] > source.size || streams.pack_sizes[k] > source.size - streams.pack_offsets[k]) {
                        _ctx.fail(errc::corrupt, "7z: a packed stream past the archive's end");
                        return;
                    }
                }
                _main = _build(streams, f.main, 0);
                _check = f.has_crc;
            }

            const std::optional<Failure>& failure() const noexcept {
                return _ctx.failure;
            }

            uint64_t size() const noexcept {
                return _folder.size();
            }

            uint64_t done() const noexcept {
                return _done;
            }

            // Up to n bytes of the folder's output, got 0 at its end; the
            // folder's CRC checked at its last byte
            bool read(uint8_t* out, size_t n, size_t& got) {
                got = 0;
                if (!_main) {
                    return false;
                }
                if (!_main->read(out, n, got)) {
                    return false;
                }
                if (_check) {
                    _crc.update(slice<const byte>(reinterpret_cast<const byte*>(out), got));
                }
                _done += got;
                if (_check && _done == size() && got) {
                    _check = false;
                    if (_crc.value() != _folder.crc) {
                        return _ctx.fail(errc::checksum, "7z: the folder's CRC-32 does not match its data");
                    }
                }
                return true;
            }

            // n bytes decoded and dropped
            bool skip(uint64_t n) {
                uint8_t scrap[16384];
                while (n) {
                    size_t got;
                    if (!read(scrap, size_t(std::min<uint64_t>(n, sizeof(scrap))), got)) {
                        return false;
                    }
                    if (got == 0) {
                        return _ctx.fail(errc::corrupt, "7z: a folder shorter than its streams");
                    }
                    n -= got;
                }
                return true;
            }

        private:
            template<class N, class... A>
            N* _make(A&&... a) {
                auto n = std::make_unique<N>(std::forward<A>(a)...);
                N* p = n.get();
                _nodes.push_back(std::move(n));
                return p;
            }

            // The node of coder c's output, its inputs made first
            Node* _build(const Streams& s, uint32_t c, uint32_t depth) {
                const Folder& f = _folder;
                if (depth > f.coders.size()) {
                    _ctx.fail(errc::corrupt, "7z: coders bound in a cycle");
                    return nullptr;
                }
                const Coder& coder = f.coders[c];
                Node* in[MaxStreams];
                for (uint32_t j = 0; j < coder.inputs; ++j) {
                    uint32_t index = coder.first_input + j;
                    in[j] = nullptr;
                    for (auto& b : f.binds) {
                        if (b.first == index) {
                            in[j] = _build(s, b.second, depth + 1);
                            if (!in[j]) {
                                return nullptr;
                            }
                        }
                    }
                    if (in[j]) {
                        continue;
                    }
                    for (size_t p = 0; p < f.packed.size(); ++p) {
                        if (f.packed[p] == index) {
                            size_t k = f.first_pack + p;
                            in[j] = _make<PackNode>(_ctx, s.pack_offsets[k], s.pack_sizes[k]);
                        }
                    }
                    if (!in[j]) {
                        _ctx.fail(errc::corrupt, "7z: a coder's input bound to nothing");
                        return nullptr;
                    }
                }
                uint64_t size = f.sizes[c];
                if (coder.inputs != (coder.method == Bcj2 ? 4u : 1u)) {
                    _ctx.fail(errc::corrupt, "7z: a coder with the wrong number of inputs");
                    return nullptr;
                }
                SimpleKind kind;
                switch (coder.method) {
                    case Copy:
                        return _make<CopyNode>(_ctx, size, in[0]);
                    case Lzma: {
                        LzmaProperties p;
                        LzmaProperties::from_byte(coder.props[0], p);
                        p.dictionary = le32(coder.props.data() + 1);
                        uint64_t window = std::min<uint64_t>(std::max(p.dictionary, lzma_model::DictionaryMin), size);
                        auto* n = _make<WindowNode<LzmaDecoder>>(_ctx, size, in[0], size_t(window));
                        n->decoder.reset(p, size);
                        return n;
                    }
                    case Lzma2: {
                        uint32_t d;
                        lzma2_dictionary(coder.props[0], d);
                        uint64_t window = std::min<uint64_t>(std::max(d, lzma_model::DictionaryMin), size);
                        auto* n = _make<WindowNode<Lzma2Decoder>>(_ctx, size, in[0], size_t(window));
                        // distances no farther than the window, which holds the whole output when smaller
                        n->decoder.reset(uint32_t(window));
                        return n;
                    }
                    case Ppmd:
                        return _make<PpmdNode>(_ctx, size, in[0], coder.props[0], le32(coder.props.data() + 1));
                    case Bzip2:
                        return _make<Bzip2Node>(_ctx, size, in[0]);
                    case Deflate:
                        return _make<DeflateNode>(_ctx, size, in[0], false);
                    case Deflate64:
                        return _make<DeflateNode>(_ctx, size, in[0], true);
                    case Bcj2:
                        return _make<Bcj2Node>(_ctx, size, in);
                    case Aes: {
                        sevenzip_aes::Props p;
                        sevenzip_aes::parse(coder.props, p);
                        auto key = _ctx.keys->get(p);
                        return _make<AesNode>(_ctx, size, in[0], key, p.iv);
                    }
                    default:
                        if (simple(coder.method, kind)) {
                            SimpleFilter filter;
                            if (kind == SimpleKind::delta) {
                                filter.init(kind, false, 0, coder.props.empty() ? 1 : uint32_t(coder.props[0]) + 1);
                            } else {
                                uint32_t start = coder.props.size() >= 4 ? le32(coder.props.data()) : 0;
                                filter.init(kind, false, start);
                            }
                            return _make<FilterNode>(_ctx, size, in[0], filter);
                        }
                        _ctx.fail(errc::unsupported, "7z: unsupported method");
                        return nullptr;
                }
            }

            const Folder& _folder;
            Context _ctx;
            std::vector<std::unique_ptr<Node>> _nodes;
            Node* _main = nullptr;
            hash::crc32 _crc;
            uint64_t _done = 0;
            bool _check = false;
        };
    }
}
