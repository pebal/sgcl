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
#include "detail/content.h"
#include "detail/mime.h"
#include "../tls.h"
#include "../../async/coroutine.h"
#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../crypto/read_secret.h"
#include "../../hash/xxh3.h"
#include "../../io/file.h"
#include "../../io/fs.h"
#include "../../io/path.h"
#include "../../time/datetime.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// A directory served over HTTP in one call, a handler over https with the
// certificate and the key read from their files, and what a handler of the
// program's answers a file or content with: net::http::serve, serve_tls,
// file_server, serve_file, serve_content. The answers hold to RFC 9110's
// validators, conditional requests and ranges (detail/content.h), as Go's
// ServeContent answers them.
namespace sgcl::net::http {
    // How a served file's ETag is made
    enum class etag_kind : uint8_t {
        none,     // no ETag: the conditionals and If-Range go by Last-Modified alone
        weak,     // W/"<size>-<modified, in ns>" in hex, of the file's metadata: nothing read
        strong,   // "<XXH3-128 of the bytes>" in hex: the bytes read once, the digest kept per file
    };

    // How the files and the content are answered (serve, file_server,
    // serve_file, serve_content)
    struct serve_options {
        etag_kind etag = etag_kind::weak;   // the ETag made for a representation that has none of the handler's
        bool ranges = true;                 // Range answered (206, multipart/byteranges); false: Accept-Ranges: none, the whole body
    };

    namespace detail {
        // What fstat says of a file: the strong digests' key, its length and
        // its time
        struct FileStamp {
            uint64_t device = 0;
            uint64_t inode = 0;
            uint64_t size = 0;
            int64_t modified_ns = 0;
            bool regular = false;
            bool directory = false;

            SGCL_INLINE_HOT bool operator==(const FileStamp& o) const noexcept {
                return device == o.device && inode == o.inode && size == o.size && modified_ns == o.modified_ns;
            }
        };

        inline optional<FileStamp> file_stamp(int fd) noexcept {
            struct stat st;
            if (fd < 0 || ::fstat(fd, &st) != 0) {
                return nullopt;
            }
            FileStamp out;
            out.device = uint64_t(st.st_dev);
            out.inode = uint64_t(st.st_ino);
            out.size = uint64_t(st.st_size);
#if defined(__APPLE__)
            out.modified_ns = int64_t(st.st_mtimespec.tv_sec) * 1000000000 + st.st_mtimespec.tv_nsec;
#else
            out.modified_ns = int64_t(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec;
#endif
            out.regular = S_ISREG(st.st_mode);
            out.directory = S_ISDIR(st.st_mode);
            return out;
        }

        // v in lower-case hex, no leading zeros
        inline void append_hex(std::string& out, uint64_t v) {
            static constexpr char digits[] = "0123456789abcdef";
            char b[16];
            int n = 0;
            do {
                b[n++] = digits[v & 15];
                v >>= 4;
            } while (v);
            while (n) {
                out += b[--n];
            }
        }

        inline void append_hex(std::string& out, const array<byte, 16>& d) {
            static constexpr char digits[] = "0123456789abcdef";
            for (byte b : d) {
                out += digits[uint8_t(b) >> 4];
                out += digits[uint8_t(b) & 15];
            }
        }

        // The strong digests of the files served by the process, by what
        // fstat says of them (device, inode, size, time to the nanosecond):
        // a file's bytes read once while it stays as it is. Plain memory
        // under a mutex, never freed (a static destructor may still serve a
        // request), dropped whole past StrongDigestsMax files
        struct StrongDigests {
            struct KeyHash {
                SGCL_INLINE_HOT size_t operator()(const FileStamp& k) const noexcept {
                    uint64_t h = k.inode * 0x9E3779B97F4A7C15ull ^ k.device ^ (k.size << 1) ^ uint64_t(k.modified_ns) * 0xC2B2AE3D27D4EB4Full;
                    return size_t(h ^ (h >> 29));
                }
            };
            std::mutex lock;
            std::unordered_map<FileStamp, array<byte, 16>, KeyHash> digests;
        };

        inline constexpr size_t StrongDigestsMax = 4096;

        inline StrongDigests& strong_digests() noexcept {
            static StrongDigests* table = new StrongDigests;
            return *table;
        }

        // The digest of the file's bytes (XXH3-128), read from its start by
        // pread in blocks of plain memory; nullopt when a read fails or the
        // file changed while it was read
        inline optional<array<byte, 16>> file_digest(int fd, const FileStamp& st) noexcept {
            auto& table = strong_digests();
            {
                std::lock_guard<std::mutex> g(table.lock);
                if (auto it = table.digests.find(st); it != table.digests.end()) {
                    return it->second;
                }
            }
            constexpr size_t Block = 256 * 1024;
            auto block = std::make_unique_for_overwrite<byte[]>(Block);
            hash::xxh3_128 h;
            uint64_t at = 0;
            while (at < st.size) {
                ssize_t got = ::pread(fd, block.get(), size_t(std::min<uint64_t>(Block, st.size - at)), off_t(at));
                if (got < 0 && errno == EINTR) {
                    continue;
                }
                if (got <= 0) {
                    return nullopt;
                }
                h.update(slice<const byte>(block.get(), size_t(got)));
                at += uint64_t(got);
            }
            auto again = file_stamp(fd);
            if (!again || !(*again == st)) {
                return nullopt;   // written while it was read: no tag to vouch for its bytes
            }
            auto d = h.digest();
            std::lock_guard<std::mutex> g(table.lock);
            if (table.digests.size() >= StrongDigestsMax) {
                table.digests.clear();
            }
            table.digests.emplace(st, d);
            return d;
        }

        // The ETag of a file by the kind asked: "" for none
        inline std::string file_etag(etag_kind kind, int fd, const FileStamp& st) noexcept {
            std::string out;
            if (kind == etag_kind::weak) {
                out = "W/\"";
                append_hex(out, st.size);
                out += '-';
                append_hex(out, uint64_t(st.modified_ns));
                out += '"';
            } else if (kind == etag_kind::strong) {
                if (auto d = file_digest(fd, st)) {
                    out = "\"";
                    append_hex(out, *d);
                    out += '"';
                }
            }
            return out;
        }

        // The ETag of bytes in memory: weak, of their 64-bit digest; strong,
        // of the 128-bit one
        inline std::string bytes_etag(etag_kind kind, const slice<const byte>& data) noexcept {
            std::string out;
            if (kind == etag_kind::weak) {
                out = "W/\"";
                append_hex(out, hash::xxh3_64::of(data));
                out += '"';
            } else if (kind == etag_kind::strong) {
                out = "\"";
                append_hex(out, hash::xxh3_128::of(data));
                out += '"';
            }
            return out;
        }

        // What is answered: a regular file (its descriptor, read by pread
        // and sent by sendfile) or bytes in memory, `size` of them
        struct ServedContent {
            const io::file* file = nullptr;
            slice<const byte> bytes;
            uint64_t size = 0;
        };

        // Parts of a multipart/byteranges answer past this are not built in
        // memory: the whole body goes instead (RFC 9110 §14.2 lets a server
        // ignore a Range)
        inline constexpr uint64_t MultipartRangesMax = uint64_t(16) << 20;

        SGCL_INLINE_HOT void write_piece(WriterImpl& w, const ServedContent& c, uint64_t at, uint64_t n) {
            if (c.file) {
                w.write_region(*c.file, at, n);
            } else {
                w.touched = true;
                w.take_file();
                w.body.append(c.bytes.data() + at, size_t(n));
            }
        }

        // The answer to a request for the content, its validators known:
        // the handler's ETag and Last-Modified on the writer first, else
        // `etag` and `modified` (in whole seconds; nullopt, or the epoch,
        // for none), then the preconditions, Content-Type by the name, the
        // ranges, the body
        inline void answer_content(const request& req, response_writer w, std::string_view name, const ServedContent& c,
                                   const serve_options& o, std::string etag, optional<int64_t> modified) {
            auto& wi = *WriterAccess::impl(w);
            http::headers& out = wi.fields;
            if (auto own = HeadersAccess::find(out, "etag")) {
                etag.assign(*own);
            } else if (!etag.empty()) {
                out.set("ETag", string(std::string_view(etag)));
            }
            if (auto lm = out.date("Last-Modified")) {
                modified = lm->unix();
            } else if (modified && *modified != 0) {
                out.set_date("Last-Modified", time::datetime::from_unix(*modified, time::zone::utc()));
            }
            if (modified && *modified == 0) {
                modified.reset();   // the epoch is no time (Go's isZeroTime)
            }
            const string method = req.method();
            const std::string_view m = method.view();
            const http::headers& in = RequestAccess::impl(req)->fields;
            switch (check_preconditions(in, m, etag, modified)) {
                case Precondition::not_modified:
                    out.erase("Content-Type");
                    out.erase("Content-Length");
                    out.erase("Content-Encoding");
                    if (!etag.empty()) {
                        out.erase("Last-Modified");
                    }
                    wi.status = status::not_modified;
                    wi.touched = true;
                    return;
                case Precondition::failed:
                    wi.status = status::precondition_failed;
                    wi.touched = true;
                    return;
                case Precondition::proceed:
                    break;
            }
            if (!HeadersAccess::count(out, "content-type")) {
                out.set("Content-Type", content_type_of(name));
            }
            const bool encoded = HeadersAccess::count(out, "content-encoding") != 0;
            const bool get_or_head = m == "GET" || m == "HEAD";
            RangeSet ranges;
            if (o.ranges && get_or_head) {
                if (auto field = HeadersAccess::find(in, "range"); field && if_range_allows(in, etag, modified)) {
                    ranges = parse_ranges(*field, c.size);
                }
            }
            if (ranges.verdict == RangeVerdict::invalid || ranges.verdict == RangeVerdict::unsatisfiable) {
                out.erase("ETag");
                out.erase("Last-Modified");
                out.erase("Cache-Control");
                out.erase("Content-Encoding");
                if (ranges.verdict == RangeVerdict::unsatisfiable) {
                    out.set("Content-Range", string(std::string_view(content_range(nullptr, c.size))));
                }
                w.error(status::range_not_satisfiable);
                return;
            }
            if (!o.ranges) {
                out.set("Accept-Ranges", "none");
            } else if (!encoded) {
                out.set("Accept-Ranges", "bytes");
            }
            const bool head = m == "HEAD";
            auto length_only = [&](uint64_t n) {
                char digits[24];
                int k = std::snprintf(digits, sizeof digits, "%llu", (unsigned long long)n);
                out.set("Content-Length", string(std::string_view(digits, size_t(k))));
                wi.touched = true;
            };
            if (ranges.verdict == RangeVerdict::ranges && ranges.ranges.size() == 1) {
                const ByteRange r = ranges.ranges[0];
                wi.status = status::partial_content;
                out.set("Content-Range", string(std::string_view(content_range(&r, c.size))));
                if (head) {
                    length_only(r.length);
                } else {
                    write_piece(wi, c, r.start, r.length);
                }
                return;
            }
            if (ranges.verdict == RangeVerdict::ranges) {
                // multipart/byteranges (RFC 9110 §14.6): each part's head
                // made first, so that the length is known
                const string ctype = out.get("Content-Type");
                const string boundary = form_boundary();
                std::vector<std::string> heads;
                heads.reserve(ranges.ranges.size());
                uint64_t total = 0;
                for (size_t i = 0; i < ranges.ranges.size(); ++i) {
                    std::string h = i ? "\r\n--" : "--";
                    h += boundary.view();
                    h += "\r\n";
                    if (!ctype.empty()) {
                        h += "Content-Type: ";
                        h += ctype.view();
                        h += "\r\n";
                    }
                    h += "Content-Range: ";
                    h += content_range(&ranges.ranges[i], c.size);
                    h += "\r\n\r\n";
                    total += h.size() + ranges.ranges[i].length;
                    heads.push_back(std::move(h));
                }
                std::string tail = "\r\n--";
                tail += boundary.view();
                tail += "--\r\n";
                total += tail.size();
                uint64_t data = 0;
                for (auto& r : ranges.ranges) {
                    data += r.length;
                }
                if (data <= MultipartRangesMax) {
                    wi.status = status::partial_content;
                    out.set("Content-Type", string::concat("multipart/byteranges; boundary=", boundary));
                    if (head) {
                        length_only(total);
                        return;
                    }
                    wi.touched = true;
                    wi.take_file();
                    for (size_t i = 0; i < heads.size(); ++i) {
                        wi.body.append(heads[i]);
                        if (c.file) {
                            wi.append_region(c.file->fd(), ranges.ranges[i].start, ranges.ranges[i].length);
                        } else {
                            wi.body.append(c.bytes.data() + ranges.ranges[i].start, size_t(ranges.ranges[i].length));
                        }
                    }
                    wi.body.append(tail);
                    return;
                }
                // too much to build in memory: the whole body below
            }
            wi.status = status::ok;
            wi.touched = true;
            if (head) {
                length_only(c.size);
            } else if (c.size) {
                write_piece(wi, c, 0, c.size);
            }
        }

        // An open file answered, what fstat said of it known: its length,
        // its time and its tag (and its bytes, for a strong tag)
        inline void answer_stamped(const request& req, response_writer w, std::string_view name, const io::file& f, const FileStamp& st,
                                   const serve_options& o) {
            ServedContent c;
            c.file = &f;
            c.size = st.size;
            const int64_t seconds = st.modified_ns >= 0 ? st.modified_ns / 1000000000 : -((-st.modified_ns + 999999999) / 1000000000);
            std::string etag;
            if (!HeadersAccess::count(WriterAccess::impl(w)->fields, "etag")) {
                etag = file_etag(o.etag, f.fd(), st);
            }
            answer_content(req, w, name, c, o, std::move(etag), seconds);
        }

        // An open file answered: fstat first; one that is not a regular
        // file is 500 (a pipe has no length to range over)
        inline void answer_file(const request& req, response_writer w, std::string_view name, const io::file& f, const serve_options& o) {
            auto st = file_stamp(f.fd());
            if (!st || !st->regular) {
                w.error(status::internal_server_error);
                return;
            }
            answer_stamped(req, w, name, f, *st, o);
        }

        // A file to serve, opened: what it is, and the file and its stamp
        // when it is a regular one
        struct OpenedFile {
            enum kind_t : uint8_t { missing, directory, other, regular } kind = missing;
            io::file file;
            FileStamp stamp;
        };

        // The file at path opened for reading and its fstat, two calls of
        // the system as Go's os.Open and Stat: O_NONBLOCK, so that the open
        // of a FIFO does not wait for a writer (a regular file's reads,
        // pread and sendfile take no notice of the flag); anything but a
        // regular file closed at once
        inline OpenedFile open_served(const string& path) noexcept {
            OpenedFile out;
            int fd;
            do {
                fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            } while (fd < 0 && errno == EINTR);
            if (fd < 0) {
                return out;
            }
            auto st = file_stamp(fd);
            if (!st || !st->regular) {
                out.kind = st && st->directory ? OpenedFile::directory : OpenedFile::other;
                ::close(fd);
                return out;
            }
            out.kind = OpenedFile::regular;
            out.stamp = *st;
            out.file = io::detail::FileAccess::make(make_tracked<io::detail::FileState>(fd, path, false, true, false));
            return out;
        }

        // The file at path answered: not there, or not a regular file, 404
        inline void answer_path(const request& req, response_writer w, const string& path, const serve_options& o) {
            auto opened = open_served(path);
            if (opened.kind != OpenedFile::regular) {
                w.error(status::not_found);
                return;
            }
            answer_stamped(req, w, path.view(), opened.file, opened.stamp, o);
        }

        // One request for a file under the directory: the name through
        // io::path::under, so that no name reaches a file outside it
        // ("..%2f" included); a directory by its index.html, the URL of a
        // directory without its slash redirected (301) to it with one; a
        // name that is not a file, or not under the directory, 404. Then
        // the file answered: Content-Type by the extension, Last-Modified,
        // the ETag of the options, the conditionals and the ranges, the
        // body by sendfile over TCP
        inline void serve_under(const string& directory, const string& name, const request& req, response_writer w, const serve_options& o) {
            auto path = io::path::under(directory, name.empty() ? string(".") : name);
            if (!path) {
                w.error(status::not_found);
                return;
            }
            string served = *path;
            auto opened = open_served(served);
            if (opened.kind == OpenedFile::directory) {
                const string url_path = req.url().path();
                if (url_path.empty() || url_path.view().back() != '/') {
                    w.redirect(url_path + "/", status::moved_permanently);
                    return;
                }
                served = io::path::join(served, "index.html");
                opened = open_served(served);
            }
            if (opened.kind != OpenedFile::regular) {
                w.error(status::not_found);
                return;
            }
            answer_stamped(req, w, served.view(), opened.file, opened.stamp, o);
        }

        // The name a file server serves for a request: the route's {path...}
        // value when the route has one ("" for the directory itself), else
        // the URL's path without its first slash, unescaped
        inline string served_name(const request& req) {
            for (auto& p : RequestAccess::impl(req)->path_values) {
                if (p.first == "path") {
                    return p.second;
                }
            }
            std::string_view path = req.url().path().view();
            if (!path.empty() && path.front() == '/') {
                path.remove_prefix(1);
            }
            return string(std::string_view(net::detail::url_unescape(path)));
        }
    }

    // The handler serve() runs, for a server of the program's (Go's
    // http.FileServer): the route's {path...} value is the name under the
    // directory, or the URL's path when the route has no such wildcard:
    //
    //     srv.route("GET /static/{path...}", net::http::file_server("public"));
    //
    // A value: the directory's name and the options, copied with it
    class file_server {
    public:
        explicit file_server(const string& directory, const serve_options& o = {}) noexcept
        : _directory(directory), _options(o) {
        }

        void operator()(request req, response_writer w) const {
            detail::serve_under(_directory, detail::served_name(req), req, w, _options);
        }

    private:
        string _directory;
        serve_options _options;
    };

    // The file at path answered to the request (Go's http.ServeFile): the
    // preconditions (If-Match, If-Unmodified-Since, If-None-Match,
    // If-Modified-Since), the ranges with If-Range, Content-Type by the
    // extension unless the writer has one, Last-Modified, the ETag of the
    // options unless the writer has one. The path is used as given
    // (io::path::under keeps a user's name inside a directory); not there,
    // or not a regular file, 404
    SGCL_INLINE_HOT void serve_file(const request& req, const response_writer& w, const string& path, const serve_options& o = {}) {
        detail::answer_path(req, w, path, o);
    }

    // Content answered to the request (Go's http.ServeContent): an open
    // regular file, its length and time from fstat and its bytes from its
    // start (whatever its position), or bytes in memory. `name` gives the
    // Content-Type by its extension when the writer has none. An ETag or a
    // Last-Modified the handler set on the writer first is the validator;
    // else the options' ETag (of the file's size and time, or of the
    // bytes' digest)
    SGCL_INLINE_HOT void serve_content(const request& req, const response_writer& w, const string& name, const io::file& content, const serve_options& o = {}) {
        detail::answer_file(req, w, name.view(), content, o);
    }

    inline void serve_content(const request& req, const response_writer& w, const string& name, const slice<const byte>& content, const serve_options& o = {}) {
        detail::ServedContent c;
        c.bytes = content;
        c.size = content.size();
        std::string etag;
        if (!detail::HeadersAccess::count(detail::WriterAccess::impl(w)->fields, "etag")) {
            etag = detail::bytes_etag(o.etag, content);
        }
        detail::answer_content(req, w, name.view(), c, o, std::move(etag), nullopt);
    }

    namespace detail {
        SGCL_INLINE_HOT server file_server(const string& directory, const serve_options& o = {}) noexcept {
            server srv;
            srv.route("GET /{path...}", http::file_server(directory, o));
            return srv;
        }

        // A certificate chain and its key from their files (PEM), the key
        // read into unmanaged memory
        inline expected<net::tls::config, io::error> tls_from_files(const string& certificate_file, const string& key_file) {
            auto chain = io::read_text(certificate_file);
            if (!chain) {
                return unexpected(chain.error());
            }
            auto key = crypto::read_secret(key_file);
            if (!key) {
                return unexpected(key.error());
            }
            auto id = net::tls::identity::from_pem(*chain, *key);
            if (!id) {
                return unexpected(id.error());
            }
            net::tls::config cfg;
            cfg.identities = {*id};
            return cfg;
        }
    }

    // The files of a directory over HTTP until the server stops, as Go's
    // http.ListenAndServe(address, http.FileServer(http.Dir(directory))):
    //
    //     net::http::serve(":8080", "public");
    //
    // GET and HEAD of /a/b.txt send directory/a/b.txt; a name that would
    // leave the directory (a "..", or "..%2f" before it is unescaped) is 404
    // and nothing is opened; a directory is its index.html (no listing), a
    // file that is not there, or is not a regular file (a FIFO), 404.
    // Content-Type by the extension, Last-Modified, a weak ETag of the
    // file's size and time (the options choose), the conditional requests
    // (304, 412) and the ranges (206, multipart/byteranges, 416), the file
    // sent by sendfile. From a thread of the program; a task writes
    // `co_await net::http::async_serve(address, directory)`
    SGCL_INLINE_HOT expected<void, io::error> serve(const string& address, const string& directory, const serve_options& o = {}) {
        return detail::file_server(directory, o).serve(address);
    }

    SGCL_INLINE_HOT async::task<expected<void, io::error>> async_serve(string address, string directory, serve_options o = {}) noexcept {
        return detail::file_server(directory, o).async_serve(address);
    }

    // A handler for every path over https, the certificate chain and its
    // private key read from their PEM files (the key never in managed
    // memory), h2 and http/1.1 by ALPN, as Go's http.ListenAndServeTLS:
    //
    //     net::http::serve_tls(":8443", "cert.pem", "key.pem", [](net::http::request req, net::http::response_writer w) { ... });
    //
    // A file that cannot be read, or a key that is not the certificate's,
    // is the error, before anything listens
    template<class H>
    SGCL_INLINE_HOT expected<void, io::error> serve_tls(const string& address, const string& certificate_file, const string& key_file, H handler) {
        auto cfg = detail::tls_from_files(certificate_file, key_file);
        if (!cfg) {
            return unexpected(cfg.error());
        }
        server srv;
        srv.route("/", std::move(handler));
        return srv.serve_tls(address, *cfg);
    }

    // The same with a TLS config of the program's: an identity of its own,
    // or identity_for (acme::manager's tls_config(), certificates obtained
    // and renewed by themselves):
    //
    //     net::http::serve_tls(":443", manager.tls_config(), handler);
    //
    // The config's ALPN list completed as server::serve_tls completes it
    template<class H>
    SGCL_INLINE_HOT expected<void, io::error> serve_tls(const string& address, const net::tls::config& c, H handler) {
        server srv;
        srv.route("/", std::move(handler));
        return srv.serve_tls(address, c);
    }
}
