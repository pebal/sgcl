//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "wire.h"
#include "../../../compress/detail/deflate.h"
#include "../../../compress/detail/inflate.h"

#include <algorithm>
#include <memory>
#include <vector>

// zlib@openssh.com (RFC 4253 §6.2, delayed until the authentication
// succeeds): one zlib stream per direction for the connection's life, each
// payload compressed and flushed (a sync flush: its bytes end on a byte,
// the next payload's start where it stopped), the window kept across
// payloads. Over the compress module's DEFLATE.
namespace sgcl::net::ssh::detail {
    class ZlibOut {
    public:
        SGCL_INLINE_HOT ZlibOut() noexcept
        : _deflater(std::make_unique<compress::detail::Deflater>(6)) {
        }

        // The payload's compressed bytes into out (cleared first)
        void compress(const uint8_t* p, size_t n, Bytes& out) noexcept {
            out.clear();
            if (!_started) {
                out.push_back(0x78);   // the zlib header: DEFLATE, a 32 KB window, no dictionary
                out.push_back(0x9C);
                _started = true;
            }
            _deflater->write(p, n, out);
            _deflater->flush(out);
        }

    private:
        std::unique_ptr<compress::detail::Deflater> _deflater;
        bool _started = false;
    };

    class ZlibIn {
    public:
        SGCL_INLINE_HOT ZlibIn() noexcept
        : _state(std::make_unique<compress::detail::InflateState>())
        , _window(2 * compress::detail::WindowSize + compress::detail::MaxMatch + 8) {
            _state->reset();
        }

        // The payload's bytes decompressed into out (cleared first), at most
        // `limit` of them: false for data that is not the stream's next
        // part, a stream that ended, or more than the limit
        bool decompress(const uint8_t* in, size_t n, Bytes& out, size_t limit) noexcept {
            using namespace compress::detail;
            out.clear();
            if (_header < 2) {
                while (n && _header < 2) {
                    _head[_header++] = *in++;
                    --n;
                }
                if (_header == 2) {
                    const unsigned cmf = _head[0], flg = _head[1];
                    if ((cmf & 0x0F) != 8 || (cmf >> 4) > 7 || ((cmf << 8) | flg) % 31 != 0 || (flg & 0x20)) {
                        return false;
                    }
                }
                if (n == 0) {
                    return true;
                }
            }
            const uint8_t* end = in + n;
            const size_t window_bytes = 2 * WindowSize;
            for (;;) {
                if (_pos + MaxMatch + 8 > window_bytes) {
                    size_t keep = std::min<size_t>(_pos, WindowSize);
                    sgcl::detail::move_bytes(_window.data(), _window.data() + _pos - keep, keep);
                    _pos = keep;
                }
                size_t before = _pos;
                auto st = inflate(*_state, in, end, _window.data(), _pos, window_bytes);
                size_t made = _pos - before;
                if (made) {
                    if (out.size() + made > limit) {
                        return false;
                    }
                    size_t at = out.size();
                    out.resize(at + made);
                    sgcl::detail::copy_bytes(out.data() + at, _window.data() + before, made);
                }
                if (st == InflateStatus::failed || st == InflateStatus::done) {
                    return false;   // the stream never ends within a connection
                }
                if (st == InflateStatus::need_input && in == end) {
                    return true;
                }
            }
        }

    private:
        std::unique_ptr<compress::detail::InflateState> _state;
        std::vector<uint8_t> _window;
        size_t _pos = 0;
        uint8_t _head[2] = {};
        int _header = 0;
    };
}
