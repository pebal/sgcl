//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "types.h"
#include "../http/request.h"
#include "../http/response_writer.h"
#include "../sftp/detail/fs.h"
#include "../url.h"
#include "../../async/blocking.h"
#include "../../async/coroutine.h"
#include "../../core/aliases.h"
#include "../../core/function.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../crypto/random.h"
#include "../../encoding/xml.h"
#include "../../time/datetime.h"
#include "../../time/layout.h"

#include <cerrno>
#include <chrono>
#include <ctime>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <vector>

// A WebDAV server (RFC 4918, classes 1 and 2) of a directory, as an HTTP
// handler: PROPFIND, PROPPATCH, MKCOL, GET, HEAD, PUT, DELETE, COPY, MOVE,
// LOCK, UNLOCK, OPTIONS; every path resolved within the root as the SFTP
// server's are (sftp/detail/fs.h), so that no request reaches outside it
namespace sgcl::net::webdav {
    namespace detail {
        namespace sd = sgcl::net::sftp::detail;

        // A path's segments percent-encoded for an href: the unreserved
        // characters and "/" as they are
        inline std::string dav_escape(std::string_view p) {
            static constexpr char hex[] = "0123456789ABCDEF";
            std::string out;
            out.reserve(p.size());
            for (char ch : p) {
                uint8_t c = uint8_t(ch);
                bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~' || c == '/';
                if (keep) {
                    out += ch;
                } else {
                    out += '%';
                    out += hex[c >> 4];
                    out += hex[c & 15];
                }
            }
            return out;
        }

        // Text for XML 1.0: the markup characters escaped, and what XML
        // cannot carry (a byte that is not UTF-8, a control character but
        // tab, LF and CR, U+FFFE and U+FFFF) as U+FFFD; a file's name on a
        // disk that takes any bytes is one
        inline std::string dav_xml_escape(std::string_view s) {
            std::string out;
            out.reserve(s.size());
            size_t i = 0, n = s.size();
            while (i < n) {
                uint8_t c = uint8_t(s[i]);
                if (c < 0x80) {
                    switch (c) {
                        case '&': out += "&amp;"; break;
                        case '<': out += "&lt;"; break;
                        case '>': out += "&gt;"; break;
                        case '"': out += "&quot;"; break;
                        default:
                            if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
                                out += "\xEF\xBF\xBD";
                            } else {
                                out += char(c);
                            }
                    }
                    ++i;
                    continue;
                }
                // a sequence of UTF-8 (RFC 3629): its length, the bounds of its second byte
                size_t len = c >= 0xC2 && c <= 0xDF ? 2 : c >= 0xE0 && c <= 0xEF ? 3 : c >= 0xF0 && c <= 0xF4 ? 4 : 0;
                uint8_t lo = c == 0xE0 ? 0xA0 : c == 0xF0 ? 0x90 : 0x80;
                uint8_t hi = c == 0xED ? 0x9F : c == 0xF4 ? 0x8F : 0xBF;
                bool ok = len != 0 && i + len <= n;
                for (size_t k = 1; ok && k < len; ++k) {
                    uint8_t b = uint8_t(s[i + k]);
                    ok = k == 1 ? b >= lo && b <= hi : (b & 0xC0) == 0x80;
                }
                ok = ok && !(c == 0xEF && uint8_t(s[i + 1]) == 0xBF && (uint8_t(s[i + 2]) & 0xFE) == 0xBE);   // U+FFFE, U+FFFF
                if (ok) {
                    out.append(s.data() + i, len);
                    i += len;
                } else {
                    out += "\xEF\xBF\xBD";
                    ++i;
                }
            }
            return out;
        }

        // RFC 1123's date of a time in seconds since 1970 (getlastmodified)
        inline std::string dav_http_date(int64_t secs) {
            std::time_t t = std::time_t(secs);
            std::tm tm{};
            ::gmtime_r(&t, &tm);
            char buf[64];
            std::strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S GMT", &tm);
            return buf;
        }

        // RFC 3339's (creationdate)
        inline std::string dav_iso_date(int64_t secs) {
            std::time_t t = std::time_t(secs);
            std::tm tm{};
            ::gmtime_r(&t, &tm);
            char buf[64];
            std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
            return buf;
        }

        inline std::string dav_etag(const sd::Attrs& a) {
            char buf[64];
            std::snprintf(buf, sizeof buf, "\"%llx-%x\"", (unsigned long long)a.size, unsigned(a.mtime));
            return buf;
        }

        // The content type of a name's extension; application/octet-stream otherwise
        inline std::string_view dav_content_type(std::string_view name) {
            size_t dot = name.rfind('.');
            std::string_view ext = dot == std::string_view::npos ? std::string_view() : name.substr(dot + 1);
            struct T {
                std::string_view ext, type;
            };
            static constexpr T types[] = {{"txt", "text/plain"}, {"html", "text/html"}, {"htm", "text/html"}, {"css", "text/css"}, {"js", "text/javascript"},
                                          {"json", "application/json"}, {"xml", "application/xml"}, {"pdf", "application/pdf"}, {"png", "image/png"},
                                          {"jpg", "image/jpeg"}, {"jpeg", "image/jpeg"}, {"gif", "image/gif"}, {"svg", "image/svg+xml"}, {"zip", "application/zip"},
                                          {"md", "text/markdown"}, {"csv", "text/csv"}};
            for (auto& t : types) {
                if (t.ext.size() == ext.size()) {
                    bool same = true;
                    for (size_t i = 0; i < ext.size() && same; ++i) {
                        same = char(ext[i] | 0x20) == t.ext[i];
                    }
                    if (same) {
                        return t.type;
                    }
                }
            }
            return "application/octet-stream";
        }

        struct DavLock {
            std::string token;      // "urn:uuid:..."
            std::string root;       // the locked resource's rel path
            bool deep = false;      // Depth infinity: the whole collection
            std::string owner;      // the owner element's XML, as the client sent it
            int64_t expires = 0;    // seconds since 1970; 0: never
            int64_t timeout = 0;    // the seconds it was given
        };

        struct DavSettings {
            std::string prefix;              // the URL path the root is served under ("/dav"), "" for "/"
            bool read_only = false;
            size_t max_body = size_t(1) << 30;
            function<bool(const http::request&)> authorize;
        };

        struct DavAccess;

        struct DavState {
            std::mutex lock;                 // the tree's calls, the locks
            std::unique_ptr<sd::Fs> fs;
            std::map<std::string, DavLock> locks;   // by token
            std::map<std::string, std::map<std::string, std::string>> dead;   // dead properties: rel → {"{ns}name" → XML}
        };

        inline int64_t dav_now() noexcept {
            return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        inline int dav_status_of(int e) noexcept {
            switch (e) {
                case 0: return 200;
                case ENOENT: return 404;
                case ENOTDIR: return 409;
                case EEXIST: return 405;
                case EACCES: case EPERM: case ELOOP: return 403;
                case ENOSPC: return 507;
                case ENOTEMPTY: return 409;
                case EISDIR: return 405;
                default: return 500;
            }
        }

        // The request's path as a rel path in the tree: the prefix taken
        // off, percent-decoding undone, resolved within the root
        inline int dav_rel(DavState& s, std::string_view prefix, std::string_view url_path, std::string& rel, bool follow = true) {
            std::string p = net::detail::url_unescape(url_path);
            if (!prefix.empty()) {
                if (p.compare(0, prefix.size(), prefix) != 0 || (p.size() > prefix.size() && p[prefix.size()] != '/')) {
                    return ENOENT;
                }
                p.erase(0, prefix.size());
            }
            if (p.empty()) {
                p = "/";
            }
            return sd::resolve(*s.fs, p, "/", follow, rel);
        }

        inline std::string dav_href(std::string_view prefix, const std::string& rel, bool collection) {
            std::string h(prefix);
            h += '/';
            h += rel;
            if (collection && !rel.empty()) {
                h += '/';
            }
            return dav_escape(h);
        }

        inline bool dav_is_dir(const sd::Attrs& a) noexcept {
            return (a.permissions & S_IFMT) == S_IFDIR;
        }

        // The locks that cover a path: those of the path, and the deep ones of its ancestors
        inline std::vector<const DavLock*> dav_locks_on(DavState& s, const std::string& rel) {
            std::vector<const DavLock*> out;
            int64_t now = dav_now();
            for (auto it = s.locks.begin(); it != s.locks.end();) {
                if (it->second.expires && it->second.expires < now) {
                    it = s.locks.erase(it);
                    continue;
                }
                const DavLock& l = it->second;
                bool covers = l.root == rel || (l.deep && (l.root.empty() || (rel.size() > l.root.size() && rel.compare(0, l.root.size(), l.root) == 0 && rel[l.root.size()] == '/')));
                if (covers) {
                    out.push_back(&l);
                }
                ++it;
            }
            return out;
        }

        // Whether the request may change the path: no lock covers it, or
        // its If header names the token of one that does (RFC 4918 §10.4)
        inline bool dav_unlocked_for(DavState& s, const std::string& rel, std::string_view if_header) {
            auto held = dav_locks_on(s, rel);
            if (held.empty()) {
                return true;
            }
            for (auto* l : held) {
                if (if_header.find("<" + l->token + ">") != std::string_view::npos) {
                    return true;
                }
            }
            return false;
        }

        // A lock taken on a collection also covers what is made under it: the
        // locks of a path's members as of a delete or a move of the whole
        inline bool dav_subtree_unlocked(DavState& s, const std::string& rel, std::string_view if_header) {
            if (!dav_unlocked_for(s, rel, if_header)) {
                return false;
            }
            for (auto& [t, l] : s.locks) {
                bool inside = rel.empty() || (l.root.size() > rel.size() && l.root.compare(0, rel.size(), rel) == 0 && l.root[rel.size()] == '/');
                if (inside && if_header.find("<" + l.token + ">") == std::string_view::npos) {
                    return false;
                }
            }
            return true;
        }

        inline std::string dav_lockdiscovery(DavState& s, const std::string& rel) {
            std::string out = "<D:lockdiscovery>";
            for (auto* l : dav_locks_on(s, rel)) {
                out += "<D:activelock><D:locktype><D:write/></D:locktype><D:lockscope><D:exclusive/></D:lockscope><D:depth>";
                out += l->deep ? "infinity" : "0";
                out += "</D:depth>";
                if (!l->owner.empty()) {
                    out += "<D:owner>" + l->owner + "</D:owner>";
                }
                out += "<D:timeout>" + (l->timeout ? "Second-" + std::to_string(l->timeout) : std::string("Infinite")) + "</D:timeout>";
                out += "<D:locktoken><D:href>" + l->token + "</D:href></D:locktoken>";
                out += "<D:lockroot><D:href>/" + dav_escape(l->root) + "</D:href></D:lockroot></D:activelock>";
            }
            out += "</D:lockdiscovery>";
            return out;
        }

        // A property of the resource: its XML, or none when it has no such property
        inline optional<std::string> dav_live(DavState& s, const std::string& rel, const sd::Attrs& a, std::string_view name) {
            bool dir = dav_is_dir(a);
            std::string leaf = rel.substr(rel.rfind('/') == std::string::npos ? 0 : rel.rfind('/') + 1);
            if (name == "resourcetype") {
                return std::string(dir ? "<D:resourcetype><D:collection/></D:resourcetype>" : "<D:resourcetype/>");
            }
            if (name == "displayname") {
                return "<D:displayname>" + dav_xml_escape(leaf) + "</D:displayname>";
            }
            if (name == "getlastmodified") {
                return "<D:getlastmodified>" + dav_http_date(a.mtime) + "</D:getlastmodified>";
            }
            if (name == "creationdate") {
                return "<D:creationdate>" + dav_iso_date(a.mtime) + "</D:creationdate>";
            }
            if (name == "getetag") {
                return "<D:getetag>" + dav_xml_escape(dav_etag(a)) + "</D:getetag>";
            }
            if (name == "supportedlock") {
                return std::string("<D:supportedlock><D:lockentry><D:lockscope><D:exclusive/></D:lockscope><D:locktype><D:write/></D:locktype></D:lockentry></D:supportedlock>");
            }
            if (name == "lockdiscovery") {
                return dav_lockdiscovery(s, rel);
            }
            if (!dir && name == "getcontentlength") {
                return "<D:getcontentlength>" + std::to_string(a.size) + "</D:getcontentlength>";
            }
            if (!dir && name == "getcontenttype") {
                return "<D:getcontenttype>" + std::string(dav_content_type(leaf)) + "</D:getcontenttype>";
            }
            return nullopt;
        }

        inline const std::vector<std::string_view>& dav_live_names() {
            static const std::vector<std::string_view> names = {"resourcetype", "displayname", "getlastmodified", "creationdate", "getetag",
                                                                "supportedlock", "lockdiscovery", "getcontentlength", "getcontenttype"};
            return names;
        }

        // What a PROPFIND asks: every property, their names, or these
        struct DavAsk {
            enum class kind { all, names, these } k = kind::all;
            std::vector<std::pair<std::string, std::string>> props;   // namespace, local name
        };

        inline void dav_response_of(DavState& s, const std::string& prefix, const std::string& rel, const sd::Attrs& a, const DavAsk& ask, std::string& out) {
            bool dir = dav_is_dir(a);
            out += "<D:response><D:href>" + dav_href(prefix, rel, dir) + "</D:href>";
            std::string found, missing;
            auto dead = s.dead.find(rel);
            if (ask.k == DavAsk::kind::names) {
                for (auto n : dav_live_names()) {
                    if (dir && (n == "getcontentlength" || n == "getcontenttype")) {
                        continue;
                    }
                    found += "<D:" + std::string(n) + "/>";
                }
                if (dead != s.dead.end()) {
                    for (auto& [k, v] : dead->second) {
                        size_t close = k.find('}');
                        found += "<x:" + k.substr(close + 1) + " xmlns:x=\"" + dav_xml_escape(k.substr(1, close - 1)) + "\"/>";
                    }
                }
            } else if (ask.k == DavAsk::kind::all) {
                for (auto n : dav_live_names()) {
                    if (auto v = dav_live(s, rel, a, n)) {
                        found += *v;
                    }
                }
                if (dead != s.dead.end()) {
                    for (auto& [k, v] : dead->second) {
                        found += v;
                    }
                }
            } else {
                for (auto& [ns, name] : ask.props) {
                    optional<std::string> v;
                    if (ns == "DAV:") {
                        v = dav_live(s, rel, a, name);
                    } else if (dead != s.dead.end()) {
                        auto it = dead->second.find("{" + ns + "}" + name);
                        if (it != dead->second.end()) {
                            v = it->second;
                        }
                    }
                    if (v) {
                        found += *v;
                    } else if (ns == "DAV:") {
                        missing += "<D:" + name + "/>";
                    } else {
                        missing += "<x:" + name + " xmlns:x=\"" + dav_xml_escape(ns) + "\"/>";
                    }
                }
            }
            if (!found.empty() || missing.empty()) {
                out += "<D:propstat><D:prop>" + found + "</D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat>";
            }
            if (!missing.empty()) {
                out += "<D:propstat><D:prop>" + missing + "</D:prop><D:status>HTTP/1.1 404 Not Found</D:status></D:propstat>";
            }
            out += "</D:response>";
        }

        // A file's bytes copied, or a collection's tree, within the tree
        inline int dav_copy_tree(sd::Fs& fs, const std::string& from, const std::string& to, bool deep, int depth = 0) {
            if (depth > 64) {
                return ELOOP;
            }
            sd::Attrs a;
            if (int e = fs.lstat(from, a)) {
                return e;
            }
            if (dav_is_dir(a)) {
                if (int e = fs.mkdir(to, 0755); e && e != EEXIST) {
                    return e;
                }
                if (!deep) {
                    return 0;
                }
                std::vector<sd::DirEntry> entries;
                if (int e = fs.list(from, entries)) {
                    return e;
                }
                for (auto& en : entries) {
                    if (en.name == "." || en.name == "..") {
                        continue;
                    }
                    std::string f = from.empty() ? en.name : from + "/" + en.name;
                    std::string t = to.empty() ? en.name : to + "/" + en.name;
                    if (int e = dav_copy_tree(fs, f, t, true, depth + 1)) {
                        return e;
                    }
                }
                return 0;
            }
            if ((a.permissions & S_IFMT) != S_IFREG) {
                return EACCES;   // a symlink or a device is not copied
            }
            int in = -1, out = -1;
            if (int e = fs.open(from, sd::FxfRead, 0, in)) {
                return e;
            }
            if (int e = fs.open(to, sd::FxfWrite | sd::FxfCreat | sd::FxfTrunc, 0644, out)) {
                fs.close(in);
                return e;
            }
            std::vector<uint8_t> buf(65536);
            uint64_t at = 0;
            int e = 0;
            for (;;) {
                size_t got = 0;
                if ((e = fs.read(in, at, buf.data(), buf.size(), got)) || got == 0) {
                    break;
                }
                if ((e = fs.write(out, at, buf.data(), got))) {
                    break;
                }
                at += got;
            }
            fs.close(in);
            fs.close(out);
            return e;
        }

        inline int dav_remove_tree(sd::Fs& fs, const std::string& rel, int depth = 0) {
            if (depth > 64) {
                return ELOOP;
            }
            sd::Attrs a;
            if (int e = fs.lstat(rel, a)) {
                return e;
            }
            if (!dav_is_dir(a)) {
                return fs.remove(rel);
            }
            std::vector<sd::DirEntry> entries;
            if (int e = fs.list(rel, entries)) {
                return e;
            }
            for (auto& en : entries) {
                if (en.name == "." || en.name == "..") {
                    continue;
                }
                if (int e = dav_remove_tree(fs, rel.empty() ? en.name : rel + "/" + en.name, depth + 1)) {
                    return e;
                }
            }
            return rel.empty() ? 0 : fs.rmdir(rel);
        }

        inline std::string dav_parent(const std::string& rel) {
            size_t slash = rel.rfind('/');
            return slash == std::string::npos ? std::string() : rel.substr(0, slash);
        }

        // What a request comes to: a status, the headers, the body
        struct DavOutcome {
            int status = 200;
            std::vector<std::pair<std::string, std::string>> headers;
            std::string body;
            std::string content_type;
            bool head = false;
        };

        inline std::string dav_multistatus(const std::string& responses) {
            return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<D:multistatus xmlns:D=\"DAV:\">" + responses + "</D:multistatus>";
        }

        // One request, its tree work done under the state's lock (on a
        // thread of the blocking pool: the tree's calls block)
        inline DavOutcome dav_handle(DavState& s, const DavSettings& cfg, const std::string& method, const std::string& url_path, const std::string& body,
                                     const std::string& depth_h, const std::string& destination, const std::string& overwrite_h, const std::string& if_h,
                                     const std::string& timeout_h, const std::string& lock_token_h, const std::string& host) {
            std::lock_guard g(s.lock);
            DavOutcome o;
            std::string rel;
            bool writing = method == "PUT" || method == "DELETE" || method == "MKCOL" || method == "MOVE" || method == "COPY" || method == "PROPPATCH" || method == "LOCK" ||
                           method == "UNLOCK";
            if (writing && cfg.read_only) {
                o.status = 403;
                return o;
            }
            int re = dav_rel(s, cfg.prefix, url_path, rel, method != "DELETE" && method != "MOVE" && method != "LOCK" && method != "UNLOCK");
            if (re && !(re == ENOENT && (method == "PUT" || method == "MKCOL" || method == "LOCK"))) {
                o.status = dav_status_of(re);
                return o;
            }
            if (re == ENOENT) {
                // a new resource: its parent must resolve
                std::string p = net::detail::url_unescape(url_path);
                if (!cfg.prefix.empty() && p.compare(0, cfg.prefix.size(), cfg.prefix) == 0) {
                    p.erase(0, cfg.prefix.size());
                }
                while (p.size() > 1 && p.back() == '/') {
                    p.pop_back();
                }
                size_t slash = p.rfind('/');
                std::string parent_rel;
                int pe = sd::resolve(*s.fs, slash == std::string::npos ? std::string("/") : p.substr(0, slash + 1), "/", true, parent_rel);
                if (pe) {
                    o.status = 409;   // a parent that is not there
                    return o;
                }
                std::string leaf = p.substr(slash + 1);
                if (leaf.empty() || leaf == "." || leaf == "..") {
                    o.status = 403;
                    return o;
                }
                rel = parent_rel.empty() ? leaf : parent_rel + "/" + leaf;
            }
            sd::Attrs a;
            int se = s.fs->lstat(rel, a);
            if (method == "OPTIONS") {
                o.headers.push_back({"DAV", "1, 2"});
                o.headers.push_back({"Allow", "OPTIONS, GET, HEAD, PUT, DELETE, PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, LOCK, UNLOCK"});
                o.headers.push_back({"MS-Author-Via", "DAV"});
                return o;
            }
            if (method == "GET" || method == "HEAD") {
                if (se) {
                    o.status = dav_status_of(se);
                    return o;
                }
                o.head = method == "HEAD";
                if (dav_is_dir(a)) {
                    // a collection: a plain list of its members' names, one a line
                    std::vector<sd::DirEntry> entries;
                    if (int e = s.fs->list(rel, entries)) {
                        o.status = dav_status_of(e);
                        return o;
                    }
                    for (auto& en : entries) {
                        if (en.name != "." && en.name != "..") {
                            o.body += en.name + (dav_is_dir(en.attrs) ? "/" : "") + "\n";
                        }
                    }
                    o.content_type = "text/plain; charset=utf-8";
                    return o;
                }
                o.content_type = std::string(dav_content_type(rel));
                o.headers.push_back({"ETag", dav_etag(a)});
                o.headers.push_back({"Last-Modified", dav_http_date(a.mtime)});
                if (o.head) {
                    o.headers.push_back({"Content-Length", std::to_string(a.size)});
                    return o;
                }
                int h = -1;
                if (int e = s.fs->open(rel, sd::FxfRead, 0, h)) {
                    o.status = dav_status_of(e);
                    return o;
                }
                o.body.resize(size_t(a.size));
                uint64_t at = 0;
                while (at < a.size) {
                    size_t got = 0;
                    if (s.fs->read(h, at, reinterpret_cast<uint8_t*>(o.body.data()) + at, size_t(a.size - at), got) || got == 0) {
                        break;
                    }
                    at += got;
                }
                o.body.resize(size_t(at));
                s.fs->close(h);
                return o;
            }
            if (method == "PUT") {
                if (se == 0 && dav_is_dir(a)) {
                    o.status = 405;
                    return o;
                }
                if (!dav_unlocked_for(s, rel, if_h)) {
                    o.status = 423;
                    return o;
                }
                int h = -1;
                if (int e = s.fs->open(rel, sd::FxfWrite | sd::FxfCreat | sd::FxfTrunc, 0644, h)) {
                    o.status = dav_status_of(e);
                    return o;
                }
                int e = body.empty() ? 0 : s.fs->write(h, 0, reinterpret_cast<const uint8_t*>(body.data()), body.size());
                s.fs->close(h);
                o.status = e ? dav_status_of(e) : (se == ENOENT ? 201 : 204);
                return o;
            }
            if (method == "MKCOL") {
                if (!body.empty()) {
                    o.status = 415;   // a body MKCOL does not know (§9.3)
                    return o;
                }
                if (se == 0) {
                    o.status = 405;
                    return o;
                }
                if (!dav_unlocked_for(s, dav_parent(rel), if_h)) {
                    o.status = 423;
                    return o;
                }
                int e = s.fs->mkdir(rel, 0755);
                o.status = e ? dav_status_of(e) : 201;
                return o;
            }
            if (method == "DELETE") {
                if (se) {
                    o.status = dav_status_of(se);
                    return o;
                }
                if (rel.empty()) {
                    o.status = 403;   // the root is not deleted
                    return o;
                }
                if (!dav_subtree_unlocked(s, rel, if_h)) {
                    o.status = 423;
                    return o;
                }
                int e = dav_remove_tree(*s.fs, rel);
                if (!e) {
                    for (auto it = s.locks.begin(); it != s.locks.end();) {
                        it = it->second.root == rel || it->second.root.rfind(rel + "/", 0) == 0 ? s.locks.erase(it) : std::next(it);
                    }
                    s.dead.erase(rel);
                }
                o.status = e ? dav_status_of(e) : 204;
                return o;
            }
            if (method == "COPY" || method == "MOVE") {
                if (se) {
                    o.status = dav_status_of(se);
                    return o;
                }
                if (destination.empty()) {
                    o.status = 400;
                    return o;
                }
                // Destination: an absolute URL of this server, or an absolute path
                std::string dest_path = destination;
                if (auto u = net::url::parse(string(destination)); u && !u->scheme().empty()) {
                    if (!host.empty() && std::string(u->host().view()) != host) {
                        o.status = 502;   // another server (§9.8.5)
                        return o;
                    }
                    dest_path = std::string(u->path().view());
                }
                std::string drel;
                int de = dav_rel(s, cfg.prefix, dest_path, drel, false);
                if (de == ENOENT) {
                    std::string p = net::detail::url_unescape(dest_path);
                    if (!cfg.prefix.empty() && p.compare(0, cfg.prefix.size(), cfg.prefix) == 0) {
                        p.erase(0, cfg.prefix.size());
                    }
                    while (p.size() > 1 && p.back() == '/') {
                        p.pop_back();
                    }
                    size_t slash = p.rfind('/');
                    std::string parent_rel;
                    if (sd::resolve(*s.fs, slash == std::string::npos ? std::string("/") : p.substr(0, slash + 1), "/", true, parent_rel)) {
                        o.status = 409;
                        return o;
                    }
                    std::string leaf = p.substr(slash + 1);
                    if (leaf.empty() || leaf == "." || leaf == "..") {
                        o.status = 403;
                        return o;
                    }
                    drel = parent_rel.empty() ? leaf : parent_rel + "/" + leaf;
                } else if (de) {
                    o.status = dav_status_of(de);
                    return o;
                }
                if (drel == rel || (dav_is_dir(a) && (rel.empty() || drel.rfind(rel + "/", 0) == 0))) {
                    o.status = 403;   // onto itself, or into itself
                    return o;
                }
                bool overwrite = overwrite_h.empty() || overwrite_h == "T" || overwrite_h == "t";
                sd::Attrs da;
                bool exists = s.fs->lstat(drel, da) == 0;
                if (exists && !overwrite) {
                    o.status = 412;
                    return o;
                }
                if (!dav_subtree_unlocked(s, drel, if_h) || (method == "MOVE" && !dav_subtree_unlocked(s, rel, if_h))) {
                    o.status = 423;
                    return o;
                }
                if (exists) {
                    if (int e = dav_remove_tree(*s.fs, drel)) {
                        o.status = dav_status_of(e);
                        return o;
                    }
                }
                int e;
                if (method == "MOVE") {
                    e = s.fs->rename(rel, drel, true);
                    if (e == EXDEV || e == ENOTSUP) {
                        e = dav_copy_tree(*s.fs, rel, drel, true);
                        if (!e) {
                            e = dav_remove_tree(*s.fs, rel);
                        }
                    }
                    if (!e) {
                        auto it = s.dead.find(rel);
                        if (it != s.dead.end()) {
                            s.dead[drel] = it->second;
                            s.dead.erase(it);
                        }
                        for (auto lt = s.locks.begin(); lt != s.locks.end();) {   // a move takes the source's locks away (§9.9.4)
                            lt = lt->second.root == rel || lt->second.root.rfind(rel + "/", 0) == 0 ? s.locks.erase(lt) : std::next(lt);
                        }
                    }
                } else {
                    e = dav_copy_tree(*s.fs, rel, drel, depth_h != "0");
                    if (!e) {
                        auto it = s.dead.find(rel);
                        if (it != s.dead.end()) {
                            s.dead[drel] = it->second;
                        }
                    }
                }
                o.status = e ? dav_status_of(e) : (exists ? 204 : 201);
                return o;
            }
            if (method == "PROPFIND") {
                if (se) {
                    o.status = dav_status_of(se);
                    return o;
                }
                DavAsk ask;
                if (!body.empty()) {
                    auto doc = encoding::xml::parse(string(body));
                    if (!doc || doc->local_name().view() != "propfind" || doc->namespace_uri().view() != "DAV:") {
                        o.status = 400;
                        return o;
                    }
                    for (auto& c : doc->children()) {
                        if (!c.is_element() || c.namespace_uri().view() != "DAV:") {
                            continue;
                        }
                        if (c.local_name().view() == "propname") {
                            ask.k = DavAsk::kind::names;
                        } else if (c.local_name().view() == "prop") {
                            ask.k = DavAsk::kind::these;
                            for (auto& p : c.children()) {
                                if (p.is_element()) {
                                    ask.props.push_back({std::string(p.namespace_uri().view()), std::string(p.local_name().view())});
                                }
                            }
                        }
                    }
                }
                std::string depth = depth_h.empty() ? "infinity" : depth_h;
                if (depth != "0" && depth != "1") {
                    // Depth infinity refused (§9.1: a server may), so that one request cannot walk the whole tree
                    o.status = 403;
                    o.content_type = "application/xml; charset=utf-8";
                    o.body = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<D:error xmlns:D=\"DAV:\"><D:propfind-finite-depth/></D:error>";
                    return o;
                }
                std::string responses;
                dav_response_of(s, cfg.prefix, rel, a, ask, responses);
                if (depth == "1" && dav_is_dir(a)) {
                    std::vector<sd::DirEntry> entries;
                    if (int e = s.fs->list(rel, entries)) {
                        o.status = dav_status_of(e);
                        return o;
                    }
                    for (auto& en : entries) {
                        if (en.name == "." || en.name == "..") {
                            continue;
                        }
                        dav_response_of(s, cfg.prefix, rel.empty() ? en.name : rel + "/" + en.name, en.attrs, ask, responses);
                    }
                }
                o.status = 207;
                o.content_type = "application/xml; charset=utf-8";
                o.body = dav_multistatus(responses);
                return o;
            }
            if (method == "PROPPATCH") {
                if (se) {
                    o.status = dav_status_of(se);
                    return o;
                }
                if (!dav_unlocked_for(s, rel, if_h)) {
                    o.status = 423;
                    return o;
                }
                auto doc = encoding::xml::parse(string(body));
                if (!doc || doc->local_name().view() != "propertyupdate") {
                    o.status = 400;
                    return o;
                }
                // dead properties set and removed in order, all of them or none (§9.2): live ones refused
                std::string ok, refused;
                auto& props = s.dead[rel];
                auto saved = props;
                for (auto& op : doc->children()) {
                    if (!op.is_element() || (op.local_name().view() != "set" && op.local_name().view() != "remove")) {
                        continue;
                    }
                    bool set = op.local_name().view() == "set";
                    for (auto& holder : op.children()) {
                        if (!holder.is_element() || holder.local_name().view() != "prop") {
                            continue;
                        }
                        for (auto& p : holder.children()) {
                            if (!p.is_element()) {
                                continue;
                            }
                            std::string ns(p.namespace_uri().view());
                            std::string name(p.local_name().view());
                            std::string tag = ns == "DAV:" ? "<D:" + name + "/>" : "<x:" + name + " xmlns:x=\"" + dav_xml_escape(ns) + "\"/>";
                            if (ns == "DAV:") {
                                refused += tag;
                                continue;
                            }
                            std::string key = "{" + ns + "}" + name;
                            if (set) {
                                props[key] = "<x:" + name + " xmlns:x=\"" + dav_xml_escape(ns) + "\">" + dav_xml_escape(std::string(p.text().view())) + "</x:" + name + ">";
                            } else {
                                props.erase(key);
                            }
                            ok += tag;
                        }
                    }
                }
                std::string r = "<D:response><D:href>" + dav_href(cfg.prefix, rel, dav_is_dir(a)) + "</D:href>";
                if (!refused.empty()) {
                    props = saved;   // all or none
                    r += "<D:propstat><D:prop>" + refused + "</D:prop><D:status>HTTP/1.1 403 Forbidden</D:status></D:propstat>";
                    if (!ok.empty()) {
                        r += "<D:propstat><D:prop>" + ok + "</D:prop><D:status>HTTP/1.1 424 Failed Dependency</D:status></D:propstat>";
                    }
                } else {
                    r += "<D:propstat><D:prop>" + ok + "</D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat>";
                }
                r += "</D:response>";
                if (props.empty()) {
                    s.dead.erase(rel);
                }
                o.status = 207;
                o.content_type = "application/xml; charset=utf-8";
                o.body = dav_multistatus(r);
                return o;
            }
            if (method == "LOCK") {
                int64_t secs = 3600;
                if (timeout_h.rfind("Second-", 0) == 0) {
                    secs = std::strtoll(timeout_h.c_str() + 7, nullptr, 10);
                    if (secs <= 0 || secs > 7 * 24 * 3600) {
                        secs = 3600;
                    }
                } else if (timeout_h.rfind("Infinite", 0) == 0) {
                    secs = 0;
                }
                if (body.empty()) {
                    // a refresh: the lock of the If header's token
                    for (auto& [t, l] : s.locks) {
                        if (if_h.find("<" + t + ">") != std::string::npos && (l.root == rel || rel.rfind(l.root + "/", 0) == 0 || l.root.empty())) {
                            l.timeout = secs;
                            l.expires = secs ? dav_now() + secs : 0;
                            o.status = 200;
                            o.content_type = "application/xml; charset=utf-8";
                            o.body = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<D:prop xmlns:D=\"DAV:\">" + dav_lockdiscovery(s, l.root) + "</D:prop>";
                            return o;
                        }
                    }
                    o.status = 412;
                    return o;
                }
                auto doc = encoding::xml::parse(string(body));
                if (!doc || doc->local_name().view() != "lockinfo") {
                    o.status = 400;
                    return o;
                }
                bool shared = false;
                std::string owner;
                for (auto& c : doc->children()) {
                    if (!c.is_element()) {
                        continue;
                    }
                    if (c.local_name().view() == "lockscope") {
                        for (auto& sc : c.children()) {
                            shared |= sc.is_element() && sc.local_name().view() == "shared";
                        }
                    } else if (c.local_name().view() == "owner") {
                        owner = dav_xml_escape(std::string(c.text().view()));
                    }
                }
                if (shared) {
                    o.status = 412;   // shared locks are not taken: exclusive write locks only
                    return o;
                }
                bool deep = depth_h.empty() || depth_h == "infinity";
                if (!dav_locks_on(s, rel).empty() || (deep && !dav_subtree_unlocked(s, rel, std::string_view()))) {
                    o.status = 423;
                    return o;
                }
                bool created = false;
                if (se == ENOENT) {
                    // a lock-null resource becomes an empty file (RFC 4918 §9.10.4)
                    int h = -1;
                    if (int e = s.fs->open(rel, sd::FxfWrite | sd::FxfCreat, 0644, h)) {
                        o.status = dav_status_of(e);
                        return o;
                    }
                    s.fs->close(h);
                    created = true;
                }
                uint8_t r[16];
                crypto::random::fill(slice<byte>(reinterpret_cast<byte*>(r), sizeof r));
                char uuid[64];
                std::snprintf(uuid, sizeof uuid, "urn:uuid:%02x%02x%02x%02x-%02x%02x-4%01x%02x-%01x%02x%02x-%02x%02x%02x%02x%02x%02x", r[0], r[1], r[2], r[3], r[4], r[5],
                              r[6] & 15, r[7], 8 | (r[8] & 3), r[9], r[10], r[11], r[12], r[13], r[14], r[15], r[0] ^ r[15]);
                DavLock l{uuid, rel, deep, owner, secs ? dav_now() + secs : 0, secs};
                s.locks[l.token] = l;
                o.status = created ? 201 : 200;
                o.headers.push_back({"Lock-Token", "<" + l.token + ">"});
                o.content_type = "application/xml; charset=utf-8";
                o.body = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<D:prop xmlns:D=\"DAV:\">" + dav_lockdiscovery(s, rel) + "</D:prop>";
                return o;
            }
            if (method == "UNLOCK") {
                std::string token = lock_token_h;
                if (token.size() >= 2 && token.front() == '<' && token.back() == '>') {
                    token = token.substr(1, token.size() - 2);
                }
                auto it = s.locks.find(token);
                if (it == s.locks.end() || !(it->second.root == rel || (it->second.deep && (it->second.root.empty() || rel.rfind(it->second.root + "/", 0) == 0)))) {
                    o.status = 409;   // no such lock on the resource (§9.11.1)
                    return o;
                }
                s.locks.erase(it);
                o.status = 204;
                return o;
            }
            o.status = 405;
            o.headers.push_back({"Allow", "OPTIONS, GET, HEAD, PUT, DELETE, PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, LOCK, UNLOCK"});
            return o;
        }
    }

    // The settings of a server: where it is mounted, what it allows
    struct server_options {
        string prefix;                                  // the URL path the root is served under ("/dav"); empty: "/"
        bool read_only = false;                         // only OPTIONS, GET, HEAD, PROPFIND: the rest 403
        size_t max_body = size_t(1) << 30;              // the largest PUT taken; past it 413
        function<bool(const http::request&)> authorize; // each request checked (its Authorization); a refusal 401; empty: every one
    };

    // A WebDAV server of a directory (RFC 4918, classes 1 and 2), an HTTP
    // handler of a route: the directory's files and collections as
    // resources, properties by PROPFIND (live ones; dead ones PROPPATCH
    // sets, kept in memory), exclusive write locks in memory. Every path is
    // resolved within the root (a ".." or a symlink that leads out stays in
    // it), as the SFTP server's. A handle of one word: copies share the
    // tree and its locks.
    //
    //     net::webdav::server dav("/srv/files", {.prefix = "/dav"});
    //     srv.route("/dav/", [dav](http::request r, http::response_writer w) { return dav.async_serve(r, w); });
    class server {
    public:
        server() noexcept = default;

        // The directory: errc of the system for one that is not there
        explicit server(const string& root, const server_options& o = {})
        : _s(make_tracked<State>()) {
            _s->dav.fs = std::make_unique<detail::sd::DiskFs>(std::string(root.view()));
            _init(o);
        }

        // An HTTP request served: its method's work on the tree, its answer written
        async::task<> async_serve(http::request r, http::response_writer w) const noexcept {
            return _co_serve(_s, std::move(r), std::move(w));
        }

        explicit operator bool() const noexcept {
            return bool(_s);
        }

        friend bool operator==(const server& a, const server& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend struct detail::DavAccess;

        struct State {
            detail::DavState dav;
            detail::DavSettings cfg;
        };

        void _init(const server_options& o) {
            std::string p(o.prefix.view());
            while (!p.empty() && p.back() == '/') {
                p.pop_back();
            }
            _s->cfg.prefix = p;
            _s->cfg.read_only = o.read_only;
            _s->cfg.max_body = o.max_body;
            _s->cfg.authorize = o.authorize;
        }

        static async::task<> _co_serve(tracked_ptr<State> s, http::request r, http::response_writer w) noexcept {
            if (s->cfg.authorize && !s->cfg.authorize(r)) {
                w.set_status(401).set_header(string("WWW-Authenticate"), string("Basic realm=\"WebDAV\""));
                co_return;
            }
            std::string method(r.method().view());
            std::string body;
            if (method == "PUT" || method == "PROPFIND" || method == "PROPPATCH" || method == "LOCK" || method == "MKCOL") {
                auto cl = r.header(string("Content-Length"));
                if (!cl.empty() && std::strtoull(cl.c_str(), nullptr, 10) > s->cfg.max_body) {
                    w.set_status(413);
                    co_return;
                }
                auto t = co_await r.async_text();
                if (!t) {
                    w.set_status(400);
                    co_return;
                }
                if (t->size() > s->cfg.max_body) {
                    w.set_status(413);
                    co_return;
                }
                body = std::string(t->view());
            }
            auto get = [&](const char* name) { return std::string(r.header(string(name)).view()); };
            std::string path(r.url().path().view());
            std::string host(r.url().host().view());
            if (host.empty()) {
                host = get("Host");
                if (auto colon = host.rfind(':'); colon != std::string::npos && host.find(']') == std::string::npos) {
                    host.erase(colon);
                }
            }
            std::string depth = get("Depth"), destination = get("Destination"), overwrite = get("Overwrite"), if_h = get("If"), timeout = get("Timeout"),
                        token = get("Lock-Token");
            tracked_ptr<State> keep = s;
            auto out = co_await async::spawn_blocking([keep, method, path, body, depth, destination, overwrite, if_h, timeout, token, host] {
                return detail::dav_handle(keep->dav, keep->cfg, method, path, body, depth, destination, overwrite, if_h, timeout, token, host);
            });
            w.set_status(out.status);
            for (auto& [k, v] : out.headers) {
                w.set_header(string(k), string(v));
            }
            if (!out.content_type.empty()) {
                w.set_header(string("Content-Type"), string(out.content_type));
            }
            if (!out.body.empty() && !out.head) {
                w.write(string(out.body));
            }
        }

        tracked_ptr<State> _s;
    };

    namespace detail {
        // The server over a tree of the tests' (sftp's MemoryFs)
        struct DavAccess {
            static server over(std::unique_ptr<sd::Fs> fs, const server_options& o = {}) {
                server s;
                s._s = make_tracked<server::State>();
                s._s->dav.fs = std::move(fs);
                s._init(o);
                return s;
            }
        };
    }
}
