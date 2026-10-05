//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/mime.h"
#include "../../async/coroutine.h"
#include "../../core/aliases.h"
#include "../../core/array.h"
#include "../../core/detail/bytes.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../crypto/random.h"
#include "../../io/file.h"
#include "../../io/fs.h"
#include "../../io/path.h"
#include "../../io/stream.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>

// A multipart/form-data body to send (RFC 7578, RFC 2046 §5.1): fields and
// files in their order, a boundary drawn at random, each file read from
// disk as the body is sent, never whole in memory.
//
//   web.post(url, net::http::form{{"name", "value"}, net::http::form::file("upload", "a.png")});
namespace sgcl::net::http {
    namespace detail {
        using namespace sgcl::detail;

        // One part: a field's text, or a file by its path
        struct FormPart {
            string name;
            string value;          // a field's text
            string path;           // a file's path; empty for a field
            string filename;       // the name the file goes by in the body
            string content_type;   // a file's type
        };

        struct FormState {
            vector<FormPart> parts;
            string boundary;
        };

        // A quoted parameter's text of Content-Disposition: a backslash
        // before '"' and '\' (a quoted-string, RFC 9110 §5.6.4, as Go and
        // curl write it), a control byte but HTAB percent-encoded (CR and
        // LF as the HTML standard has them, the others the same way), so
        // that no name ends the line or makes it a field no parser takes
        inline void quote_param(std::string& out, std::string_view v) noexcept {
            static constexpr char hex[] = "0123456789ABCDEF";
            out += '"';
            for (char c : v) {
                const auto u = uint8_t(c);
                if (c == '"' || c == '\\') {
                    out += '\\';
                    out += c;
                } else if ((u < 0x20 && c != '\t') || u == 0x7F) {
                    out += '%';
                    out += hex[u >> 4];
                    out += hex[u & 15];
                } else {
                    out += c;
                }
            }
            out += '"';
        }

        // What comes before a part's content: the delimiter (CRLF before
        // it but for the first part), the fields, the empty line
        inline std::string form_part_head(const FormState& f, size_t i) noexcept {
            const FormPart& p = f.parts[i];
            std::string h;
            h.reserve(96 + p.name.size() + p.filename.size() + p.content_type.size() + f.boundary.size());
            if (i) {
                h += "\r\n";
            }
            h += "--";
            h += f.boundary.view();
            h += "\r\nContent-Disposition: form-data; name=";
            quote_param(h, p.name.view());
            if (!p.path.empty()) {
                h += "; filename=";
                quote_param(h, p.filename.view());
                h += "\r\nContent-Type: ";
                for (char c : p.content_type.view()) {
                    h += (uint8_t(c) < 0x20 && c != '\t') || uint8_t(c) == 0x7F ? ' ' : c;   // a type never ends the line
                }
            }
            h += "\r\n\r\n";
            return h;
        }

        // The close delimiter after the last part (or alone, for no part)
        inline std::string form_tail(const FormState& f) noexcept {
            std::string t;
            if (!f.parts.empty()) {
                t += "\r\n";
            }
            t += "--";
            t += f.boundary.view();
            t += "--\r\n";
            return t;
        }

        // The body as a stream: each part's head from memory, a field's
        // text, a file's bytes read from it (opened when its part is
        // reached, closed at its end), exactly the size it had when the
        // body's length was taken: a file grown since gives that many, one
        // shrunk is io::errc::unexpected_eof
        class FormReader final : public io::mixin::reader<FormReader> {
        public:
            SGCL_INLINE_HOT FormReader(const tracked_ptr<FormState>& form, vector<uint64_t> sizes) noexcept
            : _form(form)
            , _sizes(std::move(sizes)) {
                _start(0);
            }

            expected<size_t, io::error> read(const slice<byte>& out) {
                size_t done = 0;
                while (done < out.size()) {
                    if (_phase == Phase::content && _is_file()) {
                        if (done) {
                            break;   // a file's read on its own: what is there is given now
                        }
                        if (!_file) {
                            auto f = io::open(_part().path);
                            if (!f) {
                                return io::detail::fail(f.error());
                            }
                            _file = *f;
                        }
                        auto n = _file.read(out.first(size_t(std::min<uint64_t>(out.size(), _left))));
                        if (auto r = _after_file_read(n)) {
                            return *r;
                        }
                        return *n;
                    }
                    size_t n = _copy_text(out, done);
                    if (n == 0 && _phase == Phase::done) {
                        break;
                    }
                    done += n;
                }
                return done;
            }

            async::task<expected<size_t, io::error>> async_read(slice<byte> out) noexcept {
                size_t done = 0;
                while (done < out.size()) {
                    if (_phase == Phase::content && _is_file()) {
                        if (done) {
                            break;
                        }
                        if (!_file) {
                            auto f = co_await io::async_open(_part().path);
                            if (!f) {
                                co_return io::detail::fail(f.error());
                            }
                            _file = *f;
                        }
                        auto n = co_await _file.async_read(out.first(size_t(std::min<uint64_t>(out.size(), _left))));
                        if (auto r = _after_file_read(n)) {
                            co_return *r;
                        }
                        co_return *n;
                    }
                    size_t n = _copy_text(out, done);
                    if (n == 0 && _phase == Phase::done) {
                        break;
                    }
                    done += n;
                }
                co_return done;
            }

        private:
            enum class Phase : uint8_t { head, content, done };

            SGCL_INLINE_HOT const FormPart& _part() const noexcept {
                return _form->parts[_index];
            }

            SGCL_INLINE_HOT bool _is_file() const noexcept {
                return _index < _form->parts.size() && !_part().path.empty();
            }

            // part i's head next, or the tail after the last
            void _start(size_t i) noexcept {
                _index = i;
                _at = 0;
                _phase = Phase::head;
                _text = i < _form->parts.size() ? form_part_head(*_form, i) : form_tail(*_form);
                _cur = _text;
            }

            // the part's content begun: a field's text, a file's size
            void _content() noexcept {
                _phase = Phase::content;
                _at = 0;
                if (_is_file()) {
                    _left = _sizes[_index];
                    _file = io::file();
                    if (_left == 0) {
                        _start(_index + 1);
                    }
                } else {
                    _cur = _part().value.view();   // the form's string, which the form holds
                    if (_cur.empty()) {
                        _start(_index + 1);
                    }
                }
            }

            // what of a head, a field's text or the tail goes in, from done
            size_t _copy_text(const slice<byte>& out, size_t done) noexcept {
                if (_phase == Phase::done) {
                    return 0;
                }
                size_t n = std::min(out.size() - done, _cur.size() - _at);
                sgcl::detail::copy_bytes(out.data() + done, _cur.data() + _at, n);
                _at += n;
                if (_at == _cur.size()) {
                    if (_phase == Phase::head) {
                        if (_index < _form->parts.size()) {
                            _content();
                        } else {
                            _phase = Phase::done;
                        }
                    } else {
                        _start(_index + 1);
                    }
                }
                return n;
            }

            // after a read of a file: its error, the end of the file before
            // its size, or the part done when its size is in
            optional<expected<size_t, io::error>> _after_file_read(const expected<size_t, io::error>& n) {
                if (!n) {
                    (void)_file.close();
                    return expected<size_t, io::error>(io::detail::fail(n.error()));
                }
                if (*n == 0) {
                    (void)_file.close();
                    return expected<size_t, io::error>(io::detail::fail(io::error(io::errc::unexpected_eof, "read", _part().path)));
                }
                _left -= *n;
                if (_left == 0) {
                    (void)_file.close();
                    _file = io::file();
                    _start(_index + 1);
                }
                return nullopt;
            }

            tracked_ptr<FormState> _form;
            vector<uint64_t> _sizes;
            size_t _index = 0;
            Phase _phase = Phase::head;
            std::string _text;        // a head or the tail
            std::string_view _cur;    // what is being given: _text, or a field's text in the form
            size_t _at = 0;
            io::file _file;
            uint64_t _left = 0;
        };

        // The body's length and its stream, the files' sizes taken now; the
        // error of a file that cannot be read (a directory is EISDIR)
        inline expected<pair<io::reader, uint64_t>, io::error> form_body(const tracked_ptr<FormState>& form) noexcept {
            vector<uint64_t> sizes;
            sizes.resize(form->parts.size());
            uint64_t total = form_tail(*form).size();
            for (size_t i = 0; i < form->parts.size(); ++i) {
                const FormPart& p = form->parts[i];
                total += form_part_head(*form, i).size();
                if (p.path.empty()) {
                    total += p.value.size();
                    continue;
                }
                auto st = io::stat(p.path);
                if (!st) {
                    return unexpected(st.error());
                }
                if (st->is_directory()) {
                    return unexpected(io::error(error_code(EISDIR, std::system_category()), "open", p.path));
                }
                sizes[i] = st->size;
                total += st->size;
            }
            tracked_ptr reader = make_tracked<FormReader>(form, std::move(sizes));
            return pair<io::reader, uint64_t>(io::reader(reader), total);
        }

        // 16 random bytes in hex: a boundary of 32 characters, which no
        // part's content is to hold (RFC 2046 §5.1.1: at most 70)
        inline string form_boundary() noexcept {
            unsigned char r[16];
            crypto::detail::drbg_fill(r, sizeof r);
            static constexpr char hex[] = "0123456789abcdef";
            char b[32];
            for (size_t i = 0; i < 16; ++i) {
                b[2 * i] = hex[r[i] >> 4];
                b[2 * i + 1] = hex[r[i] & 15];
            }
            return string(std::string_view(b, sizeof b));
        }

        struct FormAccess;
    }

    // A multipart/form-data body (RFC 7578): fields and files in their
    // order, sent by client::post(url, form) or request::set_body(form).
    // A handle of one word, as a request is: a copy is the same form. Its
    // boundary is drawn at random when it is made; a file is read from its
    // path when the body is sent, and its size taken then, for the
    // Content-Length.
    class form {
    public:
        // One part: a field of a name and a text ({"name", "value"}), or a
        // file (form::file)
        class part {
        public:
            SGCL_INLINE_HOT part(const string& name, const string& value) noexcept {
                _p.name = name;
                _p.value = value;
            }

        private:
            friend class form;

            part() noexcept = default;

            detail::FormPart _p;
        };

        // The file at path, sent as the field name: its name in the body
        // the path's last element, its type by its extension
        // (application/octet-stream for one not known) or as given
        SGCL_INLINE_HOT static part file(const string& name, const string& path) noexcept {
            return file(name, path, string(detail::content_type_of(path.view())));
        }

        SGCL_INLINE_HOT static part file(const string& name, const string& path, const string& content_type) noexcept {
            part p;
            p._p.name = name;
            p._p.path = path;
            p._p.filename = io::path::base(path);
            p._p.content_type = content_type;
            return p;
        }

        // No part yet
        SGCL_INLINE_HOT form() noexcept
        : _s(make_tracked<detail::FormState>()) {
            _s->boundary = detail::form_boundary();
        }

        SGCL_INLINE_HOT form(std::initializer_list<part> parts) noexcept
        : form() {
            for (const part& p : parts) {
                _s->parts.push_back(p._p);
            }
        }

        // A field after the others
        SGCL_INLINE_HOT form& add(const string& name, const string& value) noexcept {
            return add(part(name, value));
        }

        // A part after the others: a field, or a file (form::file)
        SGCL_INLINE_HOT form& add(const part& p) noexcept {
            _s->parts.push_back(p._p);
            return *this;
        }

        SGCL_INLINE_HOT string boundary() const noexcept {
            return _s->boundary;
        }

        // "multipart/form-data; boundary=...", the request's Content-Type
        SGCL_INLINE_HOT string content_type() const noexcept {
            return string::concat("multipart/form-data; boundary=", _s->boundary);
        }

        // The body's length: the files' sizes taken now; the error of a
        // file that cannot be read
        SGCL_INLINE_HOT expected<uint64_t, io::error> content_length() const noexcept {
            auto b = detail::form_body(_s);
            if (!b) {
                return unexpected(b.error());
            }
            return b->second;
        }

        // The body as a stream, its files' sizes taken now (the bytes a
        // client sends), or the error of a file that cannot be read
        SGCL_INLINE_HOT expected<io::reader, io::error> reader() const noexcept {
            auto b = detail::form_body(_s);
            if (!b) {
                return unexpected(b.error());
            }
            return b->first;
        }

    private:
        friend struct detail::FormAccess;

        tracked_ptr<detail::FormState> _s;
    };

    namespace detail {
        struct FormAccess {
            SGCL_INLINE_HOT static const tracked_ptr<FormState>& state(const form& f) noexcept {
                return f._s;
            }
        };
    }
}
