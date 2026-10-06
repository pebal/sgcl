//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "request.h"
#include "response_writer.h"
#include "server.h"
#include "status.h"
#include "detail/server_state.h"
#include "../../compress/brotli.h"
#include "../../compress/gzip.h"
#include "../../compress/level.h"
#include "../../compress/zstd.h"
#include "../../compress/zlib.h"
#include "../../core/aliases.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../io/stream.h"

#include <cstdint>
#include <string>
#include <string_view>

// Response compression of a server (net::http::compression, a middleware):
// the coding a client accepts (Accept-Encoding with q-values, RFC 9110
// §12.5.3) applied to the body of a response whose type is compressible,
// as the head is made, and to what follows a flush as it goes.
namespace sgcl::net::http {
    namespace detail {
        // Accept-Encoding read: the coding of `ours` (the server's order)
        // the client takes with q above 0 — named, or under "*" — or "" for
        // none (identity). A field that is not there takes identity alone
        // (RFC 9110 §12.5.3: no Accept-Encoding, any coding may be sent, but
        // a server compresses only for a client that asks)
        inline std::string_view choose_coding(std::string_view accept, const vector<string>& ours) noexcept {
            // the q of a coding in the field: -1 when it is not named
            auto q_of = [&](std::string_view coding) -> int {
                int found = -1;
                std::string_view list = accept;
                while (!list.empty()) {
                    const size_t comma = list.find(',');
                    std::string_view item = trim_ows(list.substr(0, comma));
                    list = comma == std::string_view::npos ? std::string_view() : list.substr(comma + 1);
                    const size_t semi = item.find(';');
                    const std::string_view name = trim_ows(item.substr(0, semi));
                    if (!iequal(name, coding)) {
                        continue;
                    }
                    int q = 1000;
                    std::string_view params = semi == std::string_view::npos ? std::string_view() : item.substr(semi + 1);
                    while (!params.empty()) {
                        const size_t s2 = params.find(';');
                        std::string_view p = trim_ows(params.substr(0, s2));
                        params = s2 == std::string_view::npos ? std::string_view() : params.substr(s2 + 1);
                        if (p.size() >= 2 && (p[0] == 'q' || p[0] == 'Q') && p[1] == '=') {
                            // qvalue = ( "0" [ "." 0*3DIGIT ] ) / ( "1" [ "." 0*3("0") ] )
                            std::string_view v = p.substr(2);
                            int value = 0;
                            if (!v.empty() && (v[0] == '0' || v[0] == '1')) {
                                value = (v[0] - '0') * 1000;
                                if (v.size() > 2 && v[1] == '.') {
                                    int scale = 100;
                                    for (size_t i = 2; i < v.size() && i < 5 && v[i] >= '0' && v[i] <= '9'; ++i) {
                                        value += (v[i] - '0') * scale;
                                        scale /= 10;
                                    }
                                }
                            }
                            q = value > 1000 ? 1000 : value;
                        }
                    }
                    found = q;
                }
                return found;
            };
            const int star = q_of("*");
            for (auto& c : ours) {
                const int q = q_of(c.view());
                if (q > 0 || (q < 0 && star > 0)) {
                    return c.view();
                }
            }
            return {};
        }

        // Whether a media type is in the list: a prefix ending in '/'
        // ("text/") or a type, compared without its parameters and case
        inline bool compressible_type(std::string_view content_type, const vector<string>& types) noexcept {
            const std::string_view type = trim_ows(content_type.substr(0, content_type.find(';')));
            for (auto& t : types) {
                const std::string_view x = t.view();
                if (!x.empty() && x.back() == '/') {
                    if (type.size() > x.size() && iequal(type.substr(0, x.size()), x)) {
                        return true;
                    }
                } else if (iequal(type, x)) {
                    return true;
                }
            }
            return false;
        }

        // Where an encoder writes: the bytes kept until the filter takes them
        struct EncodedBytes {
            std::string bytes;

            expected<size_t, io::error> write(const slice<const byte>& in) {
                bytes.append(reinterpret_cast<const char*>(in.data()), in.size());
                return in.size();
            }
        };

        // A body's coding as it goes (WriterImpl::filter): the buffered
        // bytes fed to the encoder at each flush (a sync flush, so that the
        // client decodes them at once) and at the end (the stream closed)
        template<class Encoder, class Options>
        struct EncodingFilter final : BodyFilter {
            tracked_ptr<EncodedBytes> out;
            Encoder encoder;

            explicit EncodingFilter(const Options& o)
            : out(make_tracked<EncodedBytes>()), encoder(io::writer(out), o) {
            }

            void transform(BodyBuffer& body, bool last) override {
                body.each([&](const slice<const byte>& part) {
                    (void)encoder.write(part);
                });
                body.release();
                if (last) {
                    (void)encoder.close();
                } else {
                    (void)encoder.flush();
                }
                if (!out->bytes.empty()) {
                    body.append(out->bytes.data(), out->bytes.size());
                    out->bytes.clear();
                }
            }
        };

        struct CompressionState {
            vector<string> encodings;
            compress::level gzip_level;
            compress::brotli::level brotli_level = 4;
            compress::zstd::level zstd_level = 3;
            size_t min_size = 1024;
            vector<string> types;
        };

        // The decision as the head is made: Vary for every response that
        // could have been compressed; the filter installed for one that is
        inline void compress_at_head(const CompressionState& st, std::string_view coding, WriterImpl& w) {
            const bool bodyful = !w.head_request && !WriterImpl::bodiless(w.status) && w.status != status::partial_content;
            const string ctype = w.fields.get("Content-Type");
            if (!bodyful || !compressible_type(ctype.view(), st.types) || HeadersAccess::count(w.fields, "content-encoding")) {
                return;
            }
            w.fields.add("Vary", "Accept-Encoding");
            if (coding.empty()) {
                return;
            }
            // a whole body below the least worth it goes as it is; a flushed
            // one is compressed whatever its first piece
            const uint64_t buffered = uint64_t(w.body.size()) + (w.has_file ? w.file_n : 0);
            if (w.finishing && buffered < st.min_size) {
                return;
            }
            if (coding == "gzip") {
                w.filter = make_tracked<EncodingFilter<compress::gzip::writer, compress::gzip::options>>(compress::gzip::options{st.gzip_level});
            } else if (coding == "deflate") {
                w.filter = make_tracked<EncodingFilter<compress::zlib::writer, compress::zlib::options>>(compress::zlib::options{st.gzip_level});
            } else if (coding == "br") {
                w.filter = make_tracked<EncodingFilter<compress::brotli::writer, compress::brotli::options>>(compress::brotli::options{st.brotli_level});
            } else if (coding == "zstd") {
                // RFC 9659 §3: a window of 8 MB at most, which the levels past 19 would pass
                compress::zstd::options zo{st.zstd_level};
                zo.checksum = false;   // TCP and TLS check the bytes already; a content checksum costs every response
                zo.window_log = st.zstd_level.value() > 19 ? 23 : 0;
                w.filter = make_tracked<EncodingFilter<compress::zstd::writer, compress::zstd::options>>(zo);
            } else {
                return;
            }
            w.fields.set("Content-Encoding", string(coding));
            w.fields.erase("Content-Length");
            w.fields.erase("Accept-Ranges");
            // the bytes differ from the identity's: a strong tag made weak
            // (RFC 9110 §8.8.3: a strong tag is of one representation's bytes)
            if (auto tag = HeadersAccess::find(w.fields, "etag"); tag && !(tag->size() > 1 && (*tag)[0] == 'W' && (*tag)[1] == '/')) {
                w.fields.set("ETag", string::concat("W/", *tag));
            }
        }
    }

    // Response compression, a middleware (server::use): the coding the
    // client accepts by Accept-Encoding — zstd (RFC 8878, a window of 8 MB at
    // most as RFC 9659 asks), br (RFC 7932), gzip or deflate (zlib's framing,
    // as RFC 9110 names it) — in the server's order among those it takes with q above 0, applied to a
    // response whose Content-Type is in the list and whose body is at least
    // min_size; a response that flushes is compressed as it goes, each flush
    // a sync flush the client decodes at once. Vary: Accept-Encoding on every
    // response that could have been compressed; Content-Length, Accept-Ranges
    // and a strong ETag's strength go with the identity. Not compressed: HEAD,
    // a status without a body, a 206, a response with a Content-Encoding of
    // the handler's
    //
    //     srv.use(net::http::compression());
    //
    // A handle of one word: copies share the settings
    class compression {
    public:
        struct options {
            vector<string> encodings = {string("zstd"), string("br"), string("gzip"), string("deflate")};   // the server's order
            compress::level gzip_level;                // gzip's and deflate's level: compress's default (6) unless set; 1 fastest, 9 smallest
            compress::brotli::level brotli_level = 4;  // br's quality: 4, a live response's (11 is for files compressed once)
            compress::zstd::level zstd_level = 3;      // zstd's level: 3, zstd's own default
            size_t min_size = 1024;                    // a whole body below it goes as it is
            vector<string> types = {string("text/"),          string("application/json"), string("application/javascript"),
                                    string("application/xml"), string("application/wasm"), string("image/svg+xml"),
                                    string("application/x-ndjson"), string("application/manifest+json")};   // a prefix ending in '/', or a type
        };

        // zstd, br, gzip, deflate in this order at their live levels, 1024
        // bytes and up, the compressible types
        compression()
        : compression(options()) {
        }

        explicit compression(const options& o)
        : _s(make_tracked<detail::CompressionState>()) {
            for (auto& e : o.encodings) {
                if (detail::iequal(e.view(), "gzip") || detail::iequal(e.view(), "deflate") || detail::iequal(e.view(), "br") ||
                    detail::iequal(e.view(), "zstd")) {
                    _s->encodings.push_back(e);
                }
            }
            _s->gzip_level = o.gzip_level;
            _s->brotli_level = o.brotli_level;
            _s->zstd_level = o.zstd_level;
            _s->min_size = o.min_size;
            _s->types = o.types;
        }

        SGCL_INLINE_HOT handler wrap(const handler& next) const {
            return detail::HandlerAccess::make(_wrap(detail::HandlerAccess::step(next)));
        }

    private:
        friend struct detail::MiddlewareAccess;

        detail::Step _wrap(detail::Step next) const {
            return [st = _s, next = std::move(next)](request& r, response_writer& w) -> optional<async::task<>> {
                auto& req = *detail::RequestAccess::impl(r);
                const std::string_view coding = detail::choose_coding(detail::HeadersAccess::find(req.fields, "accept-encoding").value_or(std::string_view()),
                                                                      st->encodings);
                detail::add_before_head(*detail::WriterAccess::impl(w), [st, coding = string(coding)](detail::WriterImpl& wi) {
                    detail::compress_at_head(*st, coding.view(), wi);
                });
                return next(r, w);
            };
        }

        tracked_ptr<detail::CompressionState> _s;
    };
}
