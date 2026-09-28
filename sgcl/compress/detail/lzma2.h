//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "lzma_encoder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace sgcl::compress::detail {
    // LZMA2, the data of .xz and of 7z's default method: LZMA in chunks,
    // each with a header. A control byte of 0 ends the data; 1 and 2 are a
    // chunk stored as it is (up to 64 KiB, 1 resetting the dictionary
    // first); 0x80 and above a chunk of LZMA — up to 2 MiB coded into up
    // to 64 KiB with a range coder of its own — whose bits 5 and 6 say
    // what starts over before it (nothing, the state, the state and new
    // properties, all of that and the dictionary) and whose low five bits
    // are the top of its size. The dictionary's size is not in the data:
    // the container gives it in one byte (lzma2_dictionary).

    // The dictionary a properties byte of LZMA2 stands for (2 or 3 times a
    // power of two, 4 KiB to 3 GiB, and 40 for 4 GiB - 1); false past 40
    inline bool lzma2_dictionary(uint8_t b, uint32_t& size) noexcept {
        if (b > 40) {
            return false;
        }
        size = b == 40 ? UINT32_MAX : (2 | (uint32_t(b) & 1)) << (b / 2 + 11);
        return true;
    }

    // The smallest byte whose dictionary holds n
    inline uint8_t lzma2_dictionary_byte(uint32_t n) noexcept {
        for (uint8_t b = 0; b < 40; ++b) {
            uint32_t size;
            lzma2_dictionary(b, size);
            if (size >= n) {
                return b;
            }
        }
        return 40;
    }

    class Lzma2Decoder {
    public:
        errc error = errc::corrupt;
        const char* error_text = nullptr;

        void reset(uint32_t dictionary) {
            _lzma.lzma2_init(dictionary);
            _phase = Phase::header;
            _header_size = 0;
            _need_dictionary_reset = true;
            _need_properties = true;
            _taken = 0;
            _failed = false;
            _left = 0;
        }

        // Input bytes taken so far
        uint64_t taken() const noexcept {
            return _taken;
        }

        // As LzmaDecoder::decode, to the control byte that ends the data
        template<bool Linear>
        LzmaStatus decode(uint8_t* window, size_t size, size_t& pos, size_t limit, const uint8_t*& in, const uint8_t* end, bool input_ended) {
            if (_failed) {
                return LzmaStatus::failed;
            }
            for (;;) {
                switch (_phase) {
                    case Phase::ended:
                        return LzmaStatus::done;
                    case Phase::header: {
                        if (_header_size == 0) {
                            if (in == end) {
                                return _starve(input_ended);
                            }
                            uint8_t c = *in++;
                            ++_taken;
                            _header[_header_size++] = c;
                            if (c == 0) {
                                _phase = Phase::ended;
                                return LzmaStatus::done;
                            }
                            if (c >= 3 && c < 0x80) {
                                return _fail(errc::corrupt, "lzma2: an invalid control byte");
                            }
                        }
                        uint8_t c = _header[0];
                        size_t need = c < 0x80 ? 3 : c >= 0xC0 ? 6 : 5;
                        while (_header_size < need && in < end) {
                            _header[_header_size++] = *in++;
                            ++_taken;
                        }
                        if (_header_size < need) {
                            return _starve(input_ended);
                        }
                        _header_size = 0;
                        if (!_start_chunk()) {
                            return LzmaStatus::failed;
                        }
                        break;
                    }
                    case Phase::stored: {
                        if (pos >= limit) {
                            return LzmaStatus::need_room;
                        }
                        size_t n = std::min({size_t(_left), limit - pos, size_t(end - in)});
                        if (n == 0) {
                            return _starve(input_ended);
                        }
                        std::memcpy(window + pos, in, n);
                        pos += n;
                        in += n;
                        _taken += n;
                        _left -= uint32_t(n);
                        _lzma.lzma2_skip(n);
                        if (_left == 0) {
                            _phase = Phase::header;
                        }
                        break;
                    }
                    case Phase::lzma: {
                        size_t avail = size_t(end - in);
                        bool whole = _left <= avail;
                        const uint8_t* chunk_end = whole ? in + _left : end;
                        const uint8_t* start = in;
                        auto st = _lzma.decode<Linear>(window, size, pos, limit, in, chunk_end, whole || input_ended);
                        size_t used = size_t(in - start);
                        _taken += used;
                        _left -= uint32_t(used);
                        if (st == LzmaStatus::done) {
                            if (_left != 0 || _lzma.taken() != _packed) {
                                return _fail(errc::corrupt, "lzma2: a chunk's data ends before its size");
                            }
                            _phase = Phase::header;
                            break;
                        }
                        if (st == LzmaStatus::failed) {
                            bool cut = _lzma.error == errc::unexpected_end && whole;
                            return _fail(cut ? errc::corrupt : _lzma.error, cut ? "lzma2: a chunk's data is shorter than its size says" : _lzma.error_text);
                        }
                        return st;
                    }
                }
            }
        }

    private:
        enum class Phase : uint8_t {
            header,
            stored,
            lzma,
            ended
        };

        LzmaStatus _starve(bool input_ended) {
            return input_ended ? _fail(errc::unexpected_end, "lzma2: unexpected end of the compressed data") : LzmaStatus::need_input;
        }

        LzmaStatus _fail(errc code, const char* text) noexcept {
            _failed = true;
            error = code;
            error_text = text;
            return LzmaStatus::failed;
        }

        bool _start_chunk() {
            // a dictionary reset asks for new properties (and with them a
            // state reset) before the next LZMA chunk: no repeated distance
            // reaches back past it
            uint8_t c = _header[0];
            if (c < 0x80) {
                if (c == 1) {
                    _lzma.lzma2_dictionary_reset();
                    _need_dictionary_reset = false;
                    _need_properties = true;
                } else if (_need_dictionary_reset) {
                    _fail(errc::corrupt, "lzma2: the first chunk does not reset the dictionary");
                    return false;
                }
                _left = ((uint32_t(_header[1]) << 8) | _header[2]) + 1;
                _phase = Phase::stored;
                return true;
            }
            uint32_t reset = (c >> 5) & 3;
            if (reset == 3) {
                _lzma.lzma2_dictionary_reset();
                _need_dictionary_reset = false;
                _need_properties = true;
            } else if (_need_dictionary_reset) {
                _fail(errc::corrupt, "lzma2: the first chunk does not reset the dictionary");
                return false;
            }
            if (reset >= 2) {
                LzmaProperties p;
                if (!LzmaProperties::from_byte(_header[5], p) || p.lc + p.lp > 4) {
                    _fail(errc::corrupt, "lzma2: invalid properties (lc + lp past 4)");
                    return false;
                }
                _lzma.lzma2_properties(p);
                _need_properties = false;
            } else if (_need_properties) {
                _fail(errc::corrupt, "lzma2: an LZMA chunk before any properties");
                return false;
            }
            if (reset >= 1) {
                _lzma.lzma2_state_reset();
            }
            uint32_t unpacked = ((uint32_t(c) & 0x1F) << 16 | uint32_t(_header[1]) << 8 | _header[2]) + 1;
            _packed = ((uint32_t(_header[3]) << 8) | _header[4]) + 1;
            _left = _packed;
            _lzma.lzma2_chunk(unpacked);
            _phase = Phase::lzma;
            return true;
        }

        LzmaDecoder _lzma;
        Phase _phase = Phase::header;
        uint8_t _header[6];
        size_t _header_size = 0;
        uint32_t _left = 0;          // of a stored chunk, its bytes; of an LZMA chunk, its input bytes
        uint32_t _packed = 0;
        uint64_t _taken = 0;
        bool _need_dictionary_reset = true;
        bool _need_properties = true;
        bool _failed = false;
    };

    // The encoder: LZMA in chunks, each ended where the next symbol might
    // pass 2 MiB of input or 64 KiB of output; a chunk that did not get
    // smaller is stored as it is instead, and the next one starts the
    // state over (the probabilities moved over data the decoder does not
    // decode). Settings with lc + lp past 4 are the caller's to refuse.
    class Lzma2Encoder {
    public:
        explicit Lzma2Encoder(const LzmaEncoderSettings& s)
        : _lzma(s)
        , _props(s.props) {
            _lzma.chunked();
        }

        uint32_t dictionary() const noexcept {
            return _lzma.dictionary();
        }

        void attach(const uint8_t* p, size_t n) {
            _lzma.attach(p, n);
        }

        size_t append(const uint8_t* p, size_t n) {
            return _lzma.append(p, n);
        }

        // As LzmaEncoder::run, the chunks written into out as they fill
        LzmaRun run(bool finish, std::vector<uint8_t>& out, uint64_t budget = 0) {
            for (;;) {
                if (!_in_chunk) {
                    _chunk.clear();
                    _lzma.chunk_begin();
                    _in_chunk = true;
                }
                auto st = _lzma.run(finish, _chunk, budget);
                if (st == LzmaRun::chunk) {
                    _end_chunk(out);
                    continue;
                }
                if (st == LzmaRun::done && finish) {
                    _end_chunk(out);
                }
                return st;
            }
        }

        // The end of the data, after run(true)
        void finish(std::vector<uint8_t>& out) {
            out.push_back(0);
        }

        // A new stream: the window and the tables kept
        void restart() {
            _lzma.restart();
            _chunk.clear();
            _in_chunk = false;
            _need_dictionary_reset = true;
            _need_properties = true;
            _need_state_reset = false;
        }

    private:
        void _end_chunk(std::vector<uint8_t>& out) {
            uint32_t unpacked = _lzma.chunk_end(_chunk);
            _in_chunk = false;
            if (unpacked == 0) {
                return;
            }
            size_t packed = _chunk.size();
            if (packed >= unpacked || packed > LzmaEncoder::ChunkPacked) {
                const uint8_t* data = _lzma.chunk_data();
                for (uint32_t at = 0; at < unpacked; at += StoredMax) {
                    uint32_t k = std::min(unpacked - at, StoredMax);
                    out.push_back(_need_dictionary_reset ? 1 : 2);
                    out.push_back(uint8_t((k - 1) >> 8));
                    out.push_back(uint8_t(k - 1));
                    out.insert(out.end(), data + at, data + at + k);
                    _need_dictionary_reset = false;
                }
                _lzma.reset_state();
                _need_state_reset = true;
                return;
            }
            uint8_t control = uint8_t(0x80 | ((unpacked - 1) >> 16));
            if (_need_dictionary_reset) {
                control |= 0x60;
            } else if (_need_properties) {
                control |= 0x40;
            } else if (_need_state_reset) {
                control |= 0x20;
            }
            out.push_back(control);
            out.push_back(uint8_t((unpacked - 1) >> 8));
            out.push_back(uint8_t(unpacked - 1));
            out.push_back(uint8_t((packed - 1) >> 8));
            out.push_back(uint8_t(packed - 1));
            if (control >= 0xC0) {
                out.push_back(_props.to_byte());
            }
            out.insert(out.end(), _chunk.begin(), _chunk.end());
            _need_dictionary_reset = false;
            _need_properties = false;
            _need_state_reset = false;
        }

        static constexpr uint32_t StoredMax = uint32_t(1) << 16;

        LzmaEncoder _lzma;
        LzmaProperties _props;
        std::vector<uint8_t> _chunk;
        bool _in_chunk = false;
        bool _need_dictionary_reset = true;
        bool _need_properties = true;
        bool _need_state_reset = false;
    };
}
