//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "detail/codec.h"
#include "../core/vector.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../io/stream.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace sgcl::encoding {
    namespace detail {
        using namespace sgcl::detail;

        inline constexpr char QpHex[] = "0123456789ABCDEF";

        SGCL_INLINE_HOT constexpr int qp_hex_value(uint8_t c) noexcept {
            return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        }

        // The encoding of RFC 2045 §6.7 as a machine fed in pieces: a byte
        // of 33 to 126 but '=' as itself, every other as =XX (capitals), a
        // space or a tab as itself but before a line break or the end
        // (rule 3), lines of at most 76 characters with '=' at the end of
        // a line broken by the encoding (rule 5). Text: a line break of
        // the input (CRLF, or LF alone) is CRLF, a CR alone =0D. Binary:
        // CR and LF are bytes as any other, =0D and =0A.
        struct QpEncoder {
            bool binary = false;
            bool pending_cr = false;
            int pending_ws = -1;    // a space or a tab whose fate the next byte decides
            unsigned column = 0;

            static constexpr unsigned MaxLine = 75;   // and the soft break's '=': 76

            SGCL_INLINE_HOT void _room(std::string& out, unsigned n) noexcept {
                if (column + n > MaxLine) {
                    out += "=\r\n";
                    column = 0;
                }
                column += n;
            }

            SGCL_INLINE_HOT void _literal(std::string& out, char c) noexcept {
                _room(out, 1);
                out += c;
            }

            SGCL_INLINE_HOT void _escaped(std::string& out, uint8_t c) noexcept {
                _room(out, 3);
                char e[3] = {'=', QpHex[c >> 4], QpHex[c & 15]};
                out.append(e, 3);
            }

            SGCL_INLINE_HOT void _break(std::string& out) noexcept {
                if (pending_ws >= 0) {
                    _escaped(out, uint8_t(pending_ws));
                    pending_ws = -1;
                }
                out += "\r\n";
                column = 0;
            }

            void feed(const uint8_t* p, size_t n, std::string& out) noexcept {
                out.reserve(out.size() + n + n / 8 + 8);
                const uint8_t* end = p + n;
                while (p < end) {
                    // a run of plain characters, the common case of text,
                    // copied a line's room at a time
                    if (pending_ws < 0 && !pending_cr) {
                        const uint8_t* q = p;
                        const uint8_t* stop = p + std::min<size_t>(size_t(end - p), MaxLine - column);
                        while (q < stop && *q >= 33 && *q <= 126 && *q != '=') {
                            ++q;
                        }
                        if (q != p) {
                            out.append(reinterpret_cast<const char*>(p), size_t(q - p));
                            column += unsigned(q - p);
                            p = q;
                            if (p == end) {
                                break;
                            }
                        }
                    }
                    const uint8_t c = *p++;
                    if (!binary) {
                        if (pending_cr) {
                            pending_cr = false;
                            if (c == '\n') {
                                _break(out);
                                continue;
                            }
                            if (pending_ws >= 0) {
                                _literal(out, char(pending_ws));
                                pending_ws = -1;
                            }
                            _escaped(out, '\r');
                        }
                        if (c == '\r') {
                            pending_cr = true;
                            continue;
                        }
                        if (c == '\n') {
                            _break(out);
                            continue;
                        }
                    }
                    if (pending_ws >= 0) {
                        _literal(out, char(pending_ws));
                        pending_ws = -1;
                    }
                    if (c == ' ' || c == '\t') {
                        pending_ws = c;
                    } else if (c >= 33 && c <= 126 && c != '=') {
                        _literal(out, char(c));
                    } else {
                        _escaped(out, c);
                    }
                }
            }

            void finish(std::string& out) noexcept {
                if (pending_cr) {
                    if (pending_ws >= 0) {
                        _literal(out, char(pending_ws));
                        pending_ws = -1;
                    }
                    _escaped(out, '\r');
                    pending_cr = false;
                }
                if (pending_ws >= 0) {
                    _escaped(out, uint8_t(pending_ws));
                    pending_ws = -1;
                }
            }
        };

        // The decoding as a machine fed in pieces: =XX in either case, '='
        // at the end of a line (after the white space a transport may add)
        // joins it to the next, the white space at the end of a line is
        // dropped (§6.7 rule 3: the transport's, not the text's), a line
        // break kept as it came (CRLF or LF). Strict, '=' before anything
        // else is invalid_escape at the '='; lenient, it stands for itself
        // as the RFC's note on robust decoders has it (Python's quopri).
        struct QpDecoder {
            bool lenient = false;
            enum : uint8_t { Plain, Eq, EqHex, EqSpace, EqCr } state = Plain;
            int hi = 0;
            bool pending_cr = false;
            std::string ws;            // the white space that may end the line
            std::string eq_ws;         // the white space after a '='
            uint64_t offset = 0;       // of the byte now fed, for the error
            uint64_t eq_at = 0;
            optional<error> failed;

            static constexpr size_t MaxPendingSpace = 4096;   // longer runs are the text's, written out

            SGCL_INLINE_HOT void _flush_ws(std::string& out) noexcept {
                if (!ws.empty()) {
                    out += ws;
                    ws.clear();
                }
            }

            SGCL_INLINE_HOT void _flush_cr(std::string& out) noexcept {
                if (pending_cr) {
                    out += '\r';
                    pending_cr = false;
                }
            }

            // The '=' that started an escape stands for itself (lenient),
            // with what came after it; false: strict, the error set
            bool _bad_escape(std::string& out) noexcept {
                if (!lenient) {
                    failed.emplace(errc::invalid_escape, eq_at, "invalid quoted-printable escape");
                    return false;
                }
                out += '=';
                if (state == EqHex) {
                    out += QpHex[hi];
                } else if (state == EqSpace || state == EqCr) {
                    out += eq_ws;
                    if (state == EqCr) {
                        out += '\r';
                    }
                }
                eq_ws.clear();
                state = Plain;
                return true;
            }

            // false when the input is refused (failed)
            bool feed(const uint8_t* p, size_t n, std::string& out) noexcept {
                if (failed) {
                    return false;
                }
                out.reserve(out.size() + n);
                const uint8_t* end = p + n;
                while (p < end) {
                    if (state == Plain && ws.empty() && !pending_cr) {
                        const uint8_t* q = p;
                        while (q < end && *q != '=' && *q != ' ' && *q != '\t' && *q != '\r' && *q != '\n') {
                            ++q;
                        }
                        if (q != p) {
                            out.append(reinterpret_cast<const char*>(p), size_t(q - p));
                            offset += uint64_t(q - p);
                            p = q;
                            if (p == end) {
                                break;
                            }
                        }
                    }
                    const uint8_t c = *p;
                    switch (state) {
                        case Plain:
                            if (c == '\n') {
                                ws.clear();   // the transport's
                                if (pending_cr) {
                                    out += "\r\n";
                                    pending_cr = false;
                                } else {
                                    out += '\n';
                                }
                            } else if (c == '\r') {
                                _flush_cr(out);
                                pending_cr = true;
                            } else if (c == ' ' || c == '\t') {
                                _flush_cr(out);
                                if (ws.size() >= MaxPendingSpace) {
                                    _flush_ws(out);
                                }
                                ws += char(c);
                            } else {
                                _flush_cr(out);
                                _flush_ws(out);
                                if (c == '=') {
                                    state = Eq;
                                    eq_at = offset;
                                } else {
                                    out += char(c);
                                }
                            }
                            break;
                        case Eq:
                            if (int v = qp_hex_value(c); v >= 0) {
                                hi = v;
                                state = EqHex;
                            } else if (c == '\n') {
                                state = Plain;   // a soft break
                            } else if (c == '\r') {
                                state = EqCr;
                            } else if (c == ' ' || c == '\t') {
                                state = EqSpace;
                                eq_ws += char(c);
                            } else {
                                if (!_bad_escape(out)) {
                                    return false;
                                }
                                continue;   // the byte read again as text
                            }
                            break;
                        case EqHex:
                            if (int v = qp_hex_value(c); v >= 0) {
                                out += char(hi * 16 + v);
                                state = Plain;
                            } else {
                                if (!_bad_escape(out)) {
                                    return false;
                                }
                                continue;
                            }
                            break;
                        case EqSpace:
                            if (c == ' ' || c == '\t') {
                                if (eq_ws.size() < MaxPendingSpace) {
                                    eq_ws += char(c);
                                }
                            } else if (c == '\r') {
                                state = EqCr;
                            } else if (c == '\n') {
                                eq_ws.clear();
                                state = Plain;
                            } else {
                                if (!_bad_escape(out)) {
                                    return false;
                                }
                                continue;
                            }
                            break;
                        case EqCr:
                            if (c == '\n') {
                                eq_ws.clear();
                                state = Plain;
                            } else {
                                if (!_bad_escape(out)) {
                                    return false;
                                }
                                continue;
                            }
                            break;
                    }
                    ++p;
                    ++offset;
                }
                return true;
            }

            // The end of the input: the white space at its end dropped, a
            // '=' at the very end a soft break (or with white space after
            // it), an escape cut short unexpected_end (strict) or itself
            bool finish(std::string& out) noexcept {
                if (failed) {
                    return false;
                }
                ws.clear();
                _flush_cr(out);
                if (state == EqHex || state == EqCr) {
                    if (!lenient) {
                        failed.emplace(errc::unexpected_end, offset, "quoted-printable escape cut short");
                        return false;
                    }
                    _bad_escape(out);
                }
                state = Plain;
                eq_ws.clear();
                return true;
            }
        };

        template<bool Encode>
        class QpWriter
        : public io::mixin::writer<QpWriter<Encode>> {
        public:
            using io::mixin::writer<QpWriter<Encode>>::write;
            using io::mixin::writer<QpWriter<Encode>>::async_write;

            QpWriter(bool binary, const io::writer& out) noexcept
            : _out(out) {
                _enc.binary = binary;
            }

            expected<size_t, io::error> write(const slice<const byte>& data) {
                if (auto e = _check()) {
                    return io::detail::fail(*e);
                }
                _buf.clear();
                _enc.feed(reinterpret_cast<const uint8_t*>(data.data()), data.size(), _buf);
                if (auto w = _put(); !w) {
                    return io::detail::fail(w);
                }
                return data.size();
            }

            async::task<expected<size_t, io::error>> async_write(slice<const byte> data) noexcept {
                if (auto e = _check()) {
                    co_return io::detail::fail(*e);
                }
                _buf.clear();
                _enc.feed(reinterpret_cast<const uint8_t*>(data.data()), data.size(), _buf);
                if (!_buf.empty()) {
                    auto w = co_await _out.async_write(string(_buf));
                    if (!w) {
                        _error = w.error();
                        co_return io::detail::fail(w);
                    }
                }
                co_return data.size();
            }

            expected<void, io::error> close() {
                if (_closed) {
                    return {};
                }
                if (_error) {
                    return io::detail::fail(*_error);
                }
                _closed = true;
                _buf.clear();
                _enc.finish(_buf);
                if (auto w = _put(); !w) {
                    return io::detail::fail(w);
                }
                return {};
            }

            async::task<expected<void, io::error>> async_close() noexcept {
                if (_closed) {
                    co_return expected<void, io::error>();
                }
                if (_error) {
                    co_return io::detail::fail(*_error);
                }
                _closed = true;
                _buf.clear();
                _enc.finish(_buf);
                if (!_buf.empty()) {
                    auto w = co_await _out.async_write(string(_buf));
                    if (!w) {
                        _error = w.error();
                        co_return io::detail::fail(w);
                    }
                }
                co_return expected<void, io::error>();
            }

            SGCL_INLINE_HOT bool is_closed() const noexcept {
                return _closed;
            }

        private:
            optional<io::error> _check() const noexcept {
                if (_error) {
                    return _error;
                }
                if (_closed) {
                    return io::error(io::errc::closed, "write", "quoted-printable");
                }
                return nullopt;
            }

            expected<void, io::error> _put() {
                if (_buf.empty()) {
                    return {};
                }
                auto w = _out.write(slice<const byte>(reinterpret_cast<const byte*>(_buf.data()), _buf.size()));
                if (!w) {
                    _error = w.error();
                    return io::detail::fail(w);
                }
                return {};
            }

            io::writer _out;
            QpEncoder _enc;
            std::string _buf;
            optional<io::error> _error;
            bool _closed = false;
        };

        class QpReader
        : public io::mixin::reader<QpReader> {
        public:
            QpReader(bool lenient, const io::reader& in) noexcept
            : _in(in), _block(make_tracked<CodecBlock>()) {
                _dec.lenient = lenient;
            }

            expected<size_t, io::error> read(const slice<byte>& buffer) {
                if (buffer.empty()) {
                    return 0;
                }
                for (;;) {
                    if (auto r = _give(buffer)) {
                        return std::move(*r);
                    }
                    _received(_in.read(slice<byte>(_block->data(), _block->size())));
                }
            }

            async::task<expected<size_t, io::error>> async_read(slice<byte> buffer) noexcept {
                if (buffer.empty()) {
                    co_return 0;
                }
                for (;;) {
                    if (auto r = _give(buffer)) {
                        co_return std::move(*r);
                    }
                    _received(co_await _in.async_read(slice<byte>(_block->data(), _block->size())));
                }
            }

            SGCL_INLINE_HOT const optional<error>& last_error() const noexcept {
                return _error;
            }

        private:
            // What there is to give, or nothing: more input wanted
            optional<expected<size_t, io::error>> _give(const slice<byte>& buffer) {
                if (_at < _out.size()) {
                    size_t n = std::min(buffer.size(), _out.size() - _at);
                    copy_bytes(buffer.data(), _out.data() + _at, n);
                    _at += n;
                    return expected<size_t, io::error>(n);
                }
                _out.clear();
                _at = 0;
                if (_error) {
                    return expected<size_t, io::error>(unexpected(to_io_error(*_error, "quoted-printable")));
                }
                if (_eof) {
                    return expected<size_t, io::error>(size_t(0));
                }
                return nullopt;
            }

            void _received(const expected<size_t, io::error>& r) {
                if (!r) {
                    _error.emplace(r.error(), _dec.offset);
                    return;
                }
                if (*r == 0) {
                    _eof = true;
                    if (!_dec.finish(_out)) {
                        _error = _dec.failed;
                    }
                    return;
                }
                if (!_dec.feed(reinterpret_cast<const uint8_t*>(_block->data()), *r, _out)) {
                    _error = _dec.failed;
                }
            }

            io::reader _in;
            tracked_ptr<CodecBlock> _block;
            QpDecoder _dec;
            std::string _out;
            size_t _at = 0;
            bool _eof = false;
            optional<error> _error;
        };

        // The decoded bytes of a whole text
        inline expected<vector<byte>, error> qp_decode(const char* p, size_t n, bool lenient) {
            QpDecoder d;
            d.lenient = lenient;
            std::string out;
            if (!d.feed(reinterpret_cast<const uint8_t*>(p), n, out) || !d.finish(out)) {
                return unexpected(*d.failed);
            }
            vector<byte> v;
            VectorOverwrite::resize(v, out.size());
            copy_bytes(v.data(), out.data(), out.size());
            return v;
        }

        inline string qp_encode(const uint8_t* p, size_t n, bool binary) {
            QpEncoder e;
            e.binary = binary;
            std::string out;
            e.feed(p, n, out);
            e.finish(out);
            return string(out);
        }
    }

    // Quoted-printable (RFC 2045 §6.7): text that is mostly ASCII kept
    // readable, the other bytes as =XX, lines of at most 76 characters
    // broken with a '=' at the end; the transfer encoding of a mail's text
    // that is not plain ASCII. A codec is a value, as base64's: the text
    // form (a line break of the input, CRLF or LF, is a line break of the
    // output), binary() (CR and LF are bytes like the others), strict
    // decoding (a '=' before anything but two hexadecimal digits or the
    // end of a line is invalid_escape) or lenient() (it stands for itself,
    // the robust decoder RFC 2045 recommends). The digits are written in
    // capitals and read in either case; the white space at the end of a
    // line is dropped on reading, as the transport may have added it.
    //
    //     quoted_printable::standard.encode("Grüße")   // "Gr=C3=BC=C3=9Fe"
    class quoted_printable {
    public:
        using error = encoding::error;

        class encoder;
        class decoder;

        static const quoted_printable standard;

        SGCL_INLINE_HOT constexpr quoted_printable binary() const noexcept {
            return quoted_printable(true, _lenient);
        }

        SGCL_INLINE_HOT constexpr quoted_printable lenient() const noexcept {
            return quoted_printable(_binary, true);
        }

        SGCL_INLINE_HOT constexpr bool is_binary() const noexcept {
            return _binary;
        }

        SGCL_INLINE_HOT constexpr bool is_lenient() const noexcept {
            return _lenient;
        }

        SGCL_INLINE_HOT string encode(const slice<const byte>& data) const {
            return detail::qp_encode(reinterpret_cast<const uint8_t*>(data.data()), data.size(), _binary);
        }

        SGCL_INLINE_HOT string encode(const string& text) const {
            return detail::qp_encode(reinterpret_cast<const uint8_t*>(text.data()), text.size(), _binary);
        }

        template<sgcl::detail::TextArgument T>
        SGCL_INLINE_HOT string encode(const T& text) const {
            return encode(slice<const byte>(text));
        }

        SGCL_INLINE_HOT expected<vector<byte>, error> decode(const string& text) const {
            return detail::qp_decode(text.data(), text.size(), _lenient);
        }

        template<sgcl::detail::TextArgument T>
        SGCL_INLINE_HOT expected<vector<byte>, error> decode(const T& text) const {
            const std::string_view v(text);
            return detail::qp_decode(v.data(), v.size(), _lenient);
        }

        // A writer that encodes what is written to it into out (close()
        // writes the white space held back at the end and leaves out
        // open), and a reader of the bytes the text of in decodes to
        encoder encoder_to(const io::writer& out) const noexcept;
        decoder decoder_from(const io::reader& in) const noexcept;

    private:
        SGCL_INLINE_HOT constexpr quoted_printable(bool binary, bool lenient) noexcept
        : _binary(binary), _lenient(lenient) {
        }

        bool _binary = false;
        bool _lenient = false;
    };

    inline constexpr quoted_printable quoted_printable::standard {false, false};

    class quoted_printable::encoder final
    : public detail::WriterHandle<quoted_printable::encoder, detail::QpWriter<true>> {
    public:
        using WriterHandle::WriterHandle;
    };

    class quoted_printable::decoder final
    : public detail::ReaderHandle<quoted_printable::decoder, detail::QpReader> {
    public:
        using ReaderHandle::ReaderHandle;
    };

    SGCL_INLINE_HOT quoted_printable::encoder quoted_printable::encoder_to(const io::writer& out) const noexcept {
        return detail::CodecAccess::make<encoder>(make_tracked<detail::QpWriter<true>>(_binary, out));
    }

    SGCL_INLINE_HOT quoted_printable::decoder quoted_printable::decoder_from(const io::reader& in) const noexcept {
        return detail::CodecAccess::make<decoder>(make_tracked<detail::QpReader>(_lenient, in));
    }
}

namespace sgcl::io::detail {
    template<>
    inline constexpr bool IsStreamHandle<encoding::quoted_printable::encoder> = true;

    template<>
    inline constexpr bool IsStreamHandle<encoding::quoted_printable::decoder> = true;
}
