//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cbor.h"
#include "error.h"
#include "detail/utf8_check.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/vector.h"
#include "../time/datetime.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sgcl::encoding {
    // MessagePack (msgpack/spec.md), read into and written from the value
    // of cbor, whose data model holds it: nil, booleans, integers, floats,
    // str as text, bin as bytes, arrays, maps, and the extension types as a
    // kind of their own; the timestamp (type -1) an extension that as_time
    // reads. Go's standard library has no MessagePack.
    class msgpack {
    public:
        using error = encoding::error;

        // One value of the bytes, and nothing after it: every format of the
        // spec, a str of valid UTF-8 and a map with every key once unless
        // the options allow them
        static expected<cbor, error> parse(const slice<const byte>& bytes) noexcept;
        static expected<cbor, error> parse(const slice<const byte>& bytes, const cbor::options& o) noexcept;

        // One value of a stream, and no byte past it: its formats a byte at
        // a time, so a buffered_reader under a socket; a value past
        // max_size is errc::limit_exceeded before it is held
        static expected<cbor, error> parse(const io::reader& in);
        static expected<cbor, error> parse(const io::reader& in, const cbor::options& o);
        static async::task<expected<cbor, error>> async_parse(io::reader in) noexcept;
        static async::task<expected<cbor, error>> async_parse(io::reader in, cbor::options o) noexcept;

        // The bytes of the value, each in its shortest format; a tag 1 (an
        // instant) as the timestamp extension. invalid_argument for what
        // MessagePack has no format for: undefined, a simple value, an
        // integer below -2^63, another tag (a bignum among them)
        static vector<byte> encode(const cbor& value);

        // The timestamp extension of the instant: 32 bits of seconds, 64
        // with nanoseconds, or 96, the shortest that holds it
        static cbor timestamp(const time::datetime& t) noexcept;
    };

    namespace detail {
        // The writer: an explicit stack, no recursion
        class MsgpackWriter {
        public:
            std::string out;

            void put(uint64_t v, int n) noexcept {
                for (int i = n - 1; i >= 0; --i) {
                    out += char(uint8_t(v >> (8 * i)));
                }
            }

            void length(size_t n, uint8_t fix_base, size_t fix_max, uint8_t b8, uint8_t b16, uint8_t b32) noexcept {
                if (fix_base && n <= fix_max) {
                    out += char(fix_base | n);
                } else if (b8 && n <= 0xFF) {
                    out += char(b8);
                    put(n, 1);
                } else if (n <= 0xFFFF) {
                    out += char(b16);
                    put(n, 2);
                } else {
                    out += char(b32);
                    put(n, 4);
                }
            }

            void integer(uint64_t arg, bool negative) {
                if (!negative) {
                    if (arg < 0x80) {
                        out += char(arg);
                    } else if (arg <= 0xFF) {
                        out += char(0xCC);
                        put(arg, 1);
                    } else if (arg <= 0xFFFF) {
                        out += char(0xCD);
                        put(arg, 2);
                    } else if (arg <= 0xFFFFFFFFull) {
                        out += char(0xCE);
                        put(arg, 4);
                    } else {
                        out += char(0xCF);
                        put(arg, 8);
                    }
                    return;
                }
                if (arg > uint64_t(INT64_MAX)) {
                    throw invalid_argument("sgcl::encoding::msgpack::encode: an integer below -2^63, which MessagePack has not");
                }
                int64_t v = -1 - int64_t(arg);
                if (v >= -32) {
                    out += char(uint8_t(v));
                } else if (v >= INT8_MIN) {
                    out += char(0xD0);
                    put(uint64_t(v), 1);
                } else if (v >= INT16_MIN) {
                    out += char(0xD1);
                    put(uint64_t(v), 2);
                } else if (v >= INT32_MIN) {
                    out += char(0xD2);
                    put(uint64_t(v), 4);
                } else {
                    out += char(0xD3);
                    put(uint64_t(v), 8);
                }
            }

            void extension(int8_t type, std::string_view data) noexcept {
                size_t n = data.size();
                switch (n) {
                    case 1: out += char(0xD4); break;
                    case 2: out += char(0xD5); break;
                    case 4: out += char(0xD6); break;
                    case 8: out += char(0xD7); break;
                    case 16: out += char(0xD8); break;
                    default:
                        length(n, 0, 0, 0xC7, 0xC8, 0xC9);
                        break;
                }
                out += char(type);
                out.append(data);
            }

            void write(const cbor& top);
        };

        // The timestamp extension's data of seconds and nanoseconds
        inline std::string msgpack_timestamp(int64_t seconds, uint32_t ns) noexcept {
            std::string d;
            auto put = [&](uint64_t v, int n) {
                for (int i = n - 1; i >= 0; --i) {
                    d += char(uint8_t(v >> (8 * i)));
                }
            };
            if (seconds >= 0 && (uint64_t(seconds) >> 34) == 0) {
                uint64_t v = uint64_t(ns) << 34 | uint64_t(seconds);
                if ((v & 0xFFFFFFFF00000000ull) == 0) {
                    put(v, 4);
                } else {
                    put(v, 8);
                }
            } else {
                put(ns, 4);
                put(uint64_t(seconds), 8);
            }
            return d;
        }

        inline void MsgpackWriter::write(const cbor& top) {
            struct Frame {
                const cbor* items;
                const cbor::member* members;
                size_t n;
                size_t at;
            };
            std::vector<Frame> stack;
            auto one = [&](const cbor& c) {
                switch (c.type()) {
                    case cbor::kind::null: out += char(0xC0); break;
                    case cbor::kind::boolean: out += char(*c.as_bool() ? 0xC3 : 0xC2); break;
                    case cbor::kind::integer: integer(CborAccess::bits(c), CborAccess::negative(c)); break;
                    case cbor::kind::floating: {
                        double d = *c.as_double();
                        float f = float(d);
                        if (double(f) == d || std::isnan(d)) {
                            out += char(0xCA);
                            put(std::bit_cast<uint32_t>(f), 4);
                        } else {
                            out += char(0xCB);
                            put(std::bit_cast<uint64_t>(d), 8);
                        }
                        break;
                    }
                    case cbor::kind::text: {
                        auto s = CborAccess::text(c);
                        length(s.size(), 0xA0, 31, 0xD9, 0xDA, 0xDB);
                        out.append(s.data(), s.size());
                        break;
                    }
                    case cbor::kind::bytes: {
                        auto s = CborAccess::text(c);
                        length(s.size(), 0, 0, 0xC4, 0xC5, 0xC6);
                        out.append(s.data(), s.size());
                        break;
                    }
                    case cbor::kind::extension:
                        extension(int8_t(c.tag()), CborAccess::text(c).view());
                        break;
                    case cbor::kind::array:
                        length(c.size(), 0x90, 15, 0, 0xDC, 0xDD);
                        if (c.size()) {
                            stack.push_back({CborAccess::elements(c), nullptr, c.size(), 0});
                        }
                        break;
                    case cbor::kind::map:
                        length(c.size(), 0x80, 15, 0, 0xDE, 0xDF);
                        if (c.size()) {
                            stack.push_back({nullptr, CborAccess::members(c), c.size(), 0});
                        }
                        break;
                    case cbor::kind::tag:
                        if (c.tag() == 1) {
                            if (auto t = c.as_time()) {
                                extension(-1, msgpack_timestamp(t->unix(), uint32_t(t->nanosecond())));
                                break;
                            }
                        }
                        throw invalid_argument("sgcl::encoding::msgpack::encode: a CBOR tag, which MessagePack has not");
                    case cbor::kind::undefined:
                    case cbor::kind::simple:
                        throw invalid_argument("sgcl::encoding::msgpack::encode: undefined or a simple value, which MessagePack has not");
                }
            };
            one(top);
            while (!stack.empty()) {
                Frame& f = stack.back();
                if (f.items) {
                    if (f.at == f.n) {
                        stack.pop_back();
                        continue;
                    }
                    const cbor& c = f.items[f.at++];
                    one(c);
                } else {
                    if (f.at == 2 * f.n) {
                        stack.pop_back();
                        continue;
                    }
                    const cbor::member& m = f.members[f.at / 2];
                    const cbor& c = f.at % 2 == 0 ? m.key : m.value;
                    ++f.at;
                    one(c);
                }
            }
        }

        // The reader: an explicit stack, no recursion
        class MsgpackParser {
        public:
            MsgpackParser(const uint8_t* p, size_t n, const cbor::options& o) noexcept
            : _p(p), _n(n), _o(o) {
            }

            expected<cbor, error> run() noexcept {
                struct Frame {
                    bool map;
                    uint64_t left;
                    size_t first;
                    size_t start;
                };
                std::vector<Frame> stack;
                vector<cbor> values;
                for (;;) {
                    while (!stack.empty() && stack.back().left == 0) {
                        Frame done = stack.back();
                        stack.pop_back();
                        size_t n = values.size() - done.first;
                        expected<cbor, error> made;
                        if (done.map) {
                            made = CborAccess::map_from(values, done.first, n / 2, _o, done.start);
                        } else {
                            made = n ? CborAccess::array_of(CborAccess::buffer_moved(values.data() + done.first, n), n) : CborAccess::array_of(nullptr, 0);
                        }
                        if (!made) {
                            return unexpected<error>(std::move(made.error()));
                        }
                        values.resize(done.first);
                        values.push_back(std::move(*made));
                        if (!stack.empty()) {
                            --stack.back().left;
                        }
                    }
                    if (stack.empty() && !values.empty()) {
                        break;
                    }
                    size_t start = _at;
                    if (_at >= _n) {
                        return _fail(errc::unexpected_end, _at, "unexpected end of input");
                    }
                    uint8_t b = _p[_at++];
                    uint64_t count = 0;
                    bool container = false, map = false;
                    if (b <= 0x7F) {
                        values.push_back(CborAccess::integer(b, false));
                    } else if (b <= 0x8F || (b >= 0x90 && b <= 0x9F)) {
                        container = true;
                        map = b <= 0x8F;
                        count = b & 0x0F;
                    } else if (b <= 0xBF) {
                        if (!_string(b & 0x1F, true, start, values)) {
                            return unexpected<error>(std::move(_e));
                        }
                    } else if (b >= 0xE0) {
                        values.push_back(CborAccess::integer(~uint64_t(int64_t(int8_t(b))), true));
                    } else {
                        switch (b) {
                            case 0xC0: values.push_back(cbor()); break;
                            case 0xC2: values.push_back(cbor(false)); break;
                            case 0xC3: values.push_back(cbor(true)); break;
                            case 0xC4: case 0xC5: case 0xC6: {
                                uint64_t len;
                                if (!_take(size_t(1) << (b - 0xC4), len) || !_string(len, false, start, values)) {
                                    return unexpected<error>(std::move(_e));
                                }
                                break;
                            }
                            case 0xC7: case 0xC8: case 0xC9: {
                                uint64_t len;
                                if (!_take(size_t(1) << (b - 0xC7), len) || !_extension(len, start, values)) {
                                    return unexpected<error>(std::move(_e));
                                }
                                break;
                            }
                            case 0xCA: {
                                uint64_t v;
                                if (!_take(4, v)) {
                                    return unexpected<error>(std::move(_e));
                                }
                                values.push_back(cbor(double(std::bit_cast<float>(uint32_t(v)))));
                                break;
                            }
                            case 0xCB: {
                                uint64_t v;
                                if (!_take(8, v)) {
                                    return unexpected<error>(std::move(_e));
                                }
                                values.push_back(cbor(std::bit_cast<double>(v)));
                                break;
                            }
                            case 0xCC: case 0xCD: case 0xCE: case 0xCF: {
                                uint64_t v;
                                if (!_take(size_t(1) << (b - 0xCC), v)) {
                                    return unexpected<error>(std::move(_e));
                                }
                                values.push_back(CborAccess::integer(v, false));
                                break;
                            }
                            case 0xD0: case 0xD1: case 0xD2: case 0xD3: {
                                size_t k = size_t(1) << (b - 0xD0);
                                uint64_t v;
                                if (!_take(k, v)) {
                                    return unexpected<error>(std::move(_e));
                                }
                                // sign-extended from k bytes
                                int64_t s = k == 8 ? int64_t(v) : int64_t(v << (64 - 8 * k)) >> (64 - 8 * k);
                                values.push_back(s < 0 ? CborAccess::integer(~uint64_t(s), true) : CborAccess::integer(uint64_t(s), false));
                                break;
                            }
                            case 0xD4: case 0xD5: case 0xD6: case 0xD7: case 0xD8:
                                if (!_extension(size_t(1) << (b - 0xD4), start, values)) {
                                    return unexpected<error>(std::move(_e));
                                }
                                break;
                            case 0xD9: case 0xDA: case 0xDB: {
                                uint64_t len;
                                if (!_take(size_t(1) << (b - 0xD9), len) || !_string(len, true, start, values)) {
                                    return unexpected<error>(std::move(_e));
                                }
                                break;
                            }
                            case 0xDC: case 0xDD: case 0xDE: case 0xDF:
                                if (!_take(b & 1 ? 4 : 2, count)) {
                                    return unexpected<error>(std::move(_e));
                                }
                                container = true;
                                map = b >= 0xDE;
                                break;
                            default:
                                return _fail(errc::syntax, start, "the byte 0xC1, which no format has");
                        }
                    }
                    if (container) {
                        if (stack.size() + 1 > _o.max_depth) {
                            return _fail(errc::depth_limit, start, "arrays and maps nested deeper than max_depth");
                        }
                        // each item takes a byte at least
                        if (count > _n - _at || (map && count > (_n - _at) / 2)) {
                            return _fail(errc::unexpected_end, start, "more items than the input holds");
                        }
                        stack.push_back({map, map ? count * 2 : count, values.size(), start});
                        continue;
                    }
                    if (!stack.empty()) {
                        --stack.back().left;
                    }
                }
                if (_at != _n) {
                    return _fail(errc::syntax, _at, "bytes after the value");
                }
                return std::move(values[0]);
            }

        private:
            const uint8_t* _p;
            size_t _n;
            size_t _at = 0;
            const cbor::options& _o;
            error _e;

            unexpected<error> _fail(errc code, size_t at, const char* text) noexcept {
                return unexpected<error>(error(code, at, string(text)));
            }

            bool _take(size_t k, uint64_t& v) noexcept {
                if (_n - _at < k) {
                    _e = error(errc::unexpected_end, _at, string("unexpected end of input"));
                    return false;
                }
                v = 0;
                for (size_t i = 0; i < k; ++i) {
                    v = v << 8 | _p[_at + i];
                }
                _at += k;
                return true;
            }

            bool _string(uint64_t len, bool text, size_t start, vector<cbor>& values) noexcept {
                if (len > _n - _at) {
                    _e = error(errc::unexpected_end, start, string("a string longer than the input"));
                    return false;
                }
                std::string_view v(reinterpret_cast<const char*>(_p + _at), size_t(len));
                _at += size_t(len);
                if (text && !_o.allow_invalid_utf8 && !utf8_text_valid(v.data(), v.data() + v.size())) {
                    _e = error(errc::invalid_utf8, start, string("invalid UTF-8 in a str"));
                    return false;
                }
                values.push_back(CborAccess::of_string(text ? cbor::kind::text : cbor::kind::bytes, string(v)));
                return true;
            }

            bool _extension(uint64_t len, size_t start, vector<cbor>& values) noexcept {
                if (_at >= _n || len > _n - _at - 1) {
                    _e = error(errc::unexpected_end, start, string("an extension longer than the input"));
                    return false;
                }
                int8_t type = int8_t(_p[_at++]);
                std::string_view v(reinterpret_cast<const char*>(_p + _at), size_t(len));
                _at += size_t(len);
                if (type == -1 && len != 4 && len != 8 && len != 12) {
                    _e = error(errc::syntax, start, string("a timestamp of other than 4, 8 or 12 bytes"));
                    return false;
                }
                if (type == -1 && len != 4) {
                    uint64_t w = 0;
                    for (size_t i = 0; i < (len == 8 ? 8u : 4u); ++i) {
                        w = w << 8 | uint8_t(v[i]);
                    }
                    uint64_t ns = len == 8 ? w >> 34 : w;
                    if (ns > 999999999) {
                        _e = error(errc::out_of_range, start, string("a timestamp of more than 999999999 nanoseconds"));
                        return false;
                    }
                }
                values.push_back(CborAccess::extension_of(type, string(v)));
                return true;
            }
        };
    }

    inline expected<cbor, msgpack::error> msgpack::parse(const slice<const byte>& bytes) noexcept {
        return parse(bytes, cbor::options());
    }

    inline expected<cbor, msgpack::error> msgpack::parse(const slice<const byte>& bytes, const cbor::options& o) noexcept {
        return detail::MsgpackParser(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), o).run();
    }

    inline expected<cbor, msgpack::error> msgpack::parse(const io::reader& in) {
        return parse(in, cbor::options());
    }

    inline expected<cbor, msgpack::error> msgpack::parse(const io::reader& in, const cbor::options& o) {
        auto bytes = detail::read_item(in, detail::MsgpackItemScan(o.max_depth), o.max_size);
        if (!bytes) {
            return unexpected<error>(std::move(bytes.error()));
        }
        return parse(*bytes, o);
    }

    inline async::task<expected<cbor, msgpack::error>> msgpack::async_parse(io::reader in) noexcept {
        return async_parse(std::move(in), cbor::options());
    }

    inline async::task<expected<cbor, msgpack::error>> msgpack::async_parse(io::reader in, cbor::options o) noexcept {
        auto bytes = co_await detail::async_read_item(std::move(in), detail::MsgpackItemScan(o.max_depth), o.max_size);
        if (!bytes) {
            co_return unexpected<error>(std::move(bytes.error()));
        }
        co_return parse(*bytes, o);
    }

    inline vector<byte> msgpack::encode(const cbor& value) {
        detail::MsgpackWriter w;
        w.write(value);
        vector<byte> out;
        sgcl::detail::VectorOverwrite::resize(out, w.out.size());
        sgcl::detail::copy_bytes(out.data(), w.out.data(), w.out.size());
        return out;
    }

    inline cbor msgpack::timestamp(const time::datetime& t) noexcept {
        auto d = detail::msgpack_timestamp(t.unix(), uint32_t(t.nanosecond()));
        return cbor::extension(-1, slice<const byte>(reinterpret_cast<const byte*>(d.data()), d.size()));
    }
}
