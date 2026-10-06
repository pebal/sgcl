//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../../async/coroutine.h"
#include "../../core/aliases.h"
#include "../../core/expected.h"
#include "../../core/vector.h"
#include "../../io/functions.h"
#include "../../io/stream.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// The bytes of one item of a stream of CBOR or MessagePack, and no byte past
// it: a scan that says how many bytes it wants next (a head, an argument, a
// string's data), told each time they came, and a reading that asks the
// stream for exactly those. What the bytes are is the parse's to judge: a
// malformed head ends the scan, and the parse of what was read says why.
namespace sgcl::detail {}

namespace sgcl::encoding::detail {
    using namespace sgcl::detail;

    class ItemScan {
    public:
        explicit ItemScan(uint32_t max_depth) noexcept
        : _max_depth(max_depth) {
        }

        // The bytes wanted next; 0 when the item is whole or a head was malformed
        SGCL_INLINE_HOT size_t want() const noexcept {
            return _finished || _stop ? 0 : _want;
        }

    protected:
        static constexpr uint64_t Indefinite = ~uint64_t(0);

        std::vector<uint64_t> _open;   // the items still to come in each container open
        uint32_t _max_depth;
        size_t _want = 1;
        bool _finished = false;
        bool _stop = false;

        // One item whole: the containers it closes closed with it
        void _item_done() noexcept {
            while (!_open.empty()) {
                if (_open.back() == Indefinite) {
                    return;
                }
                if (--_open.back() > 0) {
                    return;
                }
                _open.pop_back();
            }
            _finished = true;
        }

        // A container of n items opened (Indefinite until its break)
        bool _push(uint64_t n, size_t at, error& e) noexcept {
            if (n == 0) {
                _item_done();
                return true;
            }
            if (_open.size() + 1 > _max_depth) {
                e = error(errc::depth_limit, at, string("items nested deeper than max_depth"));
                return false;
            }
            _open.push_back(n);
            return true;
        }

        static uint64_t _big_endian(const uint8_t* p, size_t n) noexcept {
            uint64_t v = 0;
            for (size_t i = 0; i < n; ++i) {
                v = v << 8 | p[i];
            }
            return v;
        }
    };

    // CBOR (RFC 8949 §3): a head, its argument, a string's data
    class CborItemScan : public ItemScan {
    public:
        using ItemScan::ItemScan;

        bool take(const uint8_t* p, size_t n, size_t at, error& e) noexcept {
            switch (_state) {
                case State::head: {
                    uint8_t b = p[0];
                    if (b == 0xFF) {
                        if (!_open.empty() && _open.back() == Indefinite) {
                            _open.pop_back();
                            _item_done();
                        } else {
                            _stop = true;
                        }
                        return true;
                    }
                    _major = b >> 5;
                    uint8_t info = b & 31;
                    if (info < 24) {
                        return _dispatch(info, false, at, e);
                    }
                    if (info <= 27) {
                        _want = size_t(1) << (info - 24);
                        _state = State::argument;
                        return true;
                    }
                    if (info == 31 && _major >= 2 && _major <= 5) {
                        return _dispatch(0, true, at, e);
                    }
                    _stop = true;
                    return true;
                }
                case State::argument:
                    _state = State::head;
                    _want = 1;
                    return _dispatch(_big_endian(p, n), false, at, e);
                case State::data:
                    _state = State::head;
                    _want = 1;
                    _item_done();
                    return true;
            }
            return true;
        }

    private:
        enum class State : uint8_t { head, argument, data };
        State _state = State::head;
        uint8_t _major = 0;

        bool _dispatch(uint64_t arg, bool indefinite, size_t at, error& e) noexcept {
            switch (_major) {
                case 2:
                case 3:
                    if (indefinite) {
                        return _push(Indefinite, at, e);
                    }
                    if (arg == 0) {
                        _item_done();
                        return true;
                    }
                    if (arg > uint64_t(SIZE_MAX / 2)) {
                        e = error(errc::limit_exceeded, at, string("an item longer than max_size"));
                        return false;
                    }
                    _want = size_t(arg);
                    _state = State::data;
                    return true;
                case 4:
                    return _push(indefinite ? Indefinite : arg, at, e);
                case 5:
                    if (!indefinite && arg > (uint64_t(1) << 62)) {
                        e = error(errc::limit_exceeded, at, string("an item longer than max_size"));
                        return false;
                    }
                    return _push(indefinite ? Indefinite : arg * 2, at, e);
                case 6:
                    return _push(1, at, e);
                default:
                    _item_done();
                    return true;
            }
        }
    };

    // MessagePack (msgpack/spec.md): a format byte, a length, the data
    class MsgpackItemScan : public ItemScan {
    public:
        using ItemScan::ItemScan;

        bool take(const uint8_t* p, size_t n, size_t at, error& e) noexcept {
            switch (_state) {
                case State::head:
                    return _head(p[0], at, e);
                case State::length: {
                    uint64_t len = _big_endian(p, n);
                    _state = State::head;
                    _want = 1;
                    switch (_kind) {
                        case Kind::data:
                            return _data(len + _extra, at, e);
                        case Kind::array:
                            return _push(len, at, e);
                        case Kind::map:
                            return _push(len * 2, at, e);
                    }
                    return true;
                }
                case State::data:
                    _state = State::head;
                    _want = 1;
                    _item_done();
                    return true;
            }
            return true;
        }

    private:
        enum class State : uint8_t { head, length, data };
        enum class Kind : uint8_t { data, array, map };
        State _state = State::head;
        Kind _kind = Kind::data;
        size_t _extra = 0;   // an ext's type byte after its length

        bool _data(uint64_t len, size_t at, error& e) noexcept {
            if (len == 0) {
                _item_done();
                return true;
            }
            if (len > uint64_t(SIZE_MAX / 2)) {
                e = error(errc::limit_exceeded, at, string("an item longer than max_size"));
                return false;
            }
            _want = size_t(len);
            _state = State::data;
            return true;
        }

        bool _length(size_t k, Kind kind, size_t extra) noexcept {
            _want = k;
            _kind = kind;
            _extra = extra;
            _state = State::length;
            return true;
        }

        bool _head(uint8_t b, size_t at, error& e) noexcept {
            if (b <= 0x7F || b >= 0xE0 || b == 0xC0 || b == 0xC2 || b == 0xC3) {
                _item_done();
                return true;
            }
            if (b <= 0x8F) {
                return _push(uint64_t(b & 15) * 2, at, e);
            }
            if (b <= 0x9F) {
                return _push(b & 15, at, e);
            }
            if (b <= 0xBF) {
                return _data(b & 31, at, e);
            }
            switch (b) {
                case 0xC4: case 0xC5: case 0xC6: return _length(size_t(1) << (b - 0xC4), Kind::data, 0);
                case 0xC7: case 0xC8: case 0xC9: return _length(size_t(1) << (b - 0xC7), Kind::data, 1);
                case 0xCA: return _data(4, at, e);
                case 0xCB: return _data(8, at, e);
                case 0xCC: case 0xCD: case 0xCE: case 0xCF: return _data(size_t(1) << (b - 0xCC), at, e);
                case 0xD0: case 0xD1: case 0xD2: case 0xD3: return _data(size_t(1) << (b - 0xD0), at, e);
                case 0xD4: case 0xD5: case 0xD6: case 0xD7: case 0xD8: return _data(1 + (size_t(1) << (b - 0xD4)), at, e);
                case 0xD9: case 0xDA: case 0xDB: return _length(size_t(1) << (b - 0xD9), Kind::data, 0);
                case 0xDC: return _length(2, Kind::array, 0);
                case 0xDD: return _length(4, Kind::array, 0);
                case 0xDE: return _length(2, Kind::map, 0);
                case 0xDF: return _length(4, Kind::map, 0);
                default:
                    _stop = true;   // 0xC1: the parse says why
                    return true;
            }
        }
    };

    inline error item_read_error(const io::error& got, size_t at) noexcept {
        if (got.is_eof()) {
            return error(errc::unexpected_end, at + got.count(), string("unexpected end of the stream inside an item"));
        }
        return error(got, at + got.count());
    }

    // The bytes of one item of the stream: no byte past it is read
    template<class Scan>
    expected<vector<byte>, error> read_item(const io::reader& in, Scan scan, size_t max_size) {
        vector<byte> buffer;
        error e;
        while (size_t k = scan.want()) {
            size_t at = buffer.size();
            if (k > max_size || at + k > max_size) {
                return unexpected<error>(error(errc::limit_exceeded, at, string("an item longer than max_size")));
            }
            buffer.resize(at + k);
            auto got = io::read_full(in, buffer.as_slice(at, k));
            if (!got) {
                return unexpected<error>(item_read_error(got.error(), at));
            }
            if (*got < k) {
                return unexpected<error>(error(errc::unexpected_end, at, string(at == 0 ? "no item: the stream ended" : "unexpected end of the stream inside an item")));
            }
            if (!scan.take(reinterpret_cast<const uint8_t*>(buffer.data()) + at, k, at, e)) {
                return unexpected<error>(std::move(e));
            }
        }
        return buffer;
    }

    template<class Scan>
    async::task<expected<vector<byte>, error>> async_read_item(io::reader in, Scan scan, size_t max_size) noexcept {
        vector<byte> buffer;
        error e;
        while (size_t k = scan.want()) {
            size_t at = buffer.size();
            if (k > max_size || at + k > max_size) {
                co_return unexpected<error>(error(errc::limit_exceeded, at, string("an item longer than max_size")));
            }
            buffer.resize(at + k);
            auto got = co_await io::async_read_full(in, buffer.as_slice(at, k));
            if (!got) {
                co_return unexpected<error>(item_read_error(got.error(), at));
            }
            if (*got < k) {
                co_return unexpected<error>(error(errc::unexpected_end, at, string(at == 0 ? "no item: the stream ended" : "unexpected end of the stream inside an item")));
            }
            if (!scan.take(reinterpret_cast<const uint8_t*>(buffer.data()) + at, k, at, e)) {
                co_return unexpected<error>(std::move(e));
            }
        }
        co_return buffer;
    }
}
