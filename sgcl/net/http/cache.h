//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "headers.h"
#include "detail/cache_control.h"
#include "../../core/aliases.h"
#include "../../core/duration.h"
#include "../../core/make_tracked.h"
#include "../../core/map.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../crypto/sha256.h"
#include "../../encoding/hex.h"
#include "../../io/error.h"
#include "../../io/file.h"
#include "../../io/fs.h"
#include "../../io/path.h"
#include "../../time/datetime.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

// A private HTTP cache of a client's responses (RFC 9111): the client's
// member `cache` (client.h runs it). The entries by URL, each URL's variants
// by the request fields its Vary names; in memory, or in a directory a file
// an entry. The rules — what is stored, when it is fresh, how it is
// validated — are in detail/cache_control.h.
namespace sgcl::net::http {
    namespace detail {
        // One stored response: its request's key and Vary values, its head,
        // its body (in memory, or in its file), the times the cache saw
        struct CacheEntry {
            string url;
            vector<pair<string, string>> vary;     // the request fields Vary names, lower-case names, as they were
            int status = 200;
            http::headers fields;
            vector<byte> body;                      // in memory; empty for a disk entry (read from `file`)
            string file;                            // a disk entry's files without their extension: <file>.head, <file>.body
            bool uncompressed = false;              // the body the client decoded (Content-Encoding gone)
            uint64_t size = 0;                      // the body's bytes
            int64_t request_time = 0;
            int64_t response_time = 0;
            uint64_t used = 0;                      // the cache's clock of uses, for the least recently used
            bool revalidating = false;              // a background revalidation runs (stale-while-revalidate)
        };

        struct CacheAccess;

        struct CacheState {
            std::mutex lock;
            map<string, vector<tracked_ptr<CacheEntry>>> entries;   // by URL
            uint64_t bytes = 0;
            uint64_t clock = 0;
            uint64_t max_bytes = uint64_t(64) << 20;
            uint64_t max_entry_bytes = uint64_t(8) << 20;
            double heuristic = 0.1;
            int64_t heuristic_max = 86400;
            string directory;                       // "" in memory

            // The entry of a URL whose Vary values are the request's
            tracked_ptr<CacheEntry> find(const string& url, const http::headers& request) {
                std::lock_guard<std::mutex> g(lock);
                auto it = entries.find(url);
                if (it == entries.end()) {
                    return tracked_ptr<CacheEntry>();
                }
                for (auto& e : it->second) {
                    if (vary_matches(*e, request)) {
                        e->used = ++clock;
                        return e;
                    }
                }
                return tracked_ptr<CacheEntry>();
            }

            static bool vary_matches(const CacheEntry& e, const http::headers& request) {
                for (auto& [name, value] : e.vary) {
                    // §4.1: the values compared after their whitespace is normalized (here: trimmed, the list items joined by ", ")
                    if (normalized(request, name.view()) != value) {
                        return false;
                    }
                }
                return true;
            }

            static string normalized(const http::headers& h, std::string_view name) {
                std::string out;
                for (auto& f : HeadersAccess::fields(h)) {
                    if (!iequal(f.first.view(), name)) {
                        continue;
                    }
                    std::string_view list = f.second.view();
                    while (!list.empty()) {
                        const size_t comma = list.find(',');
                        const std::string_view item = trim_ows(list.substr(0, comma));
                        list = comma == std::string_view::npos ? std::string_view() : list.substr(comma + 1);
                        if (!item.empty()) {
                            if (!out.empty()) {
                                out += ", ";
                            }
                            out += item;
                        }
                    }
                }
                return string(std::string_view(out));
            }

            // The Vary values of a request for a response (the names it
            // lists, in lower case)
            static vector<pair<string, string>> vary_of(const http::headers& response, const http::headers& request) {
                vector<pair<string, string>> out;
                for (auto& f : HeadersAccess::fields(response)) {
                    if (!iequal(f.first.view(), "vary")) {
                        continue;
                    }
                    std::string_view list = f.second.view();
                    while (!list.empty()) {
                        const size_t comma = list.find(',');
                        std::string name(trim_ows(list.substr(0, comma)));
                        list = comma == std::string_view::npos ? std::string_view() : list.substr(comma + 1);
                        if (name.empty()) {
                            continue;
                        }
                        for (auto& c : name) {
                            c = ascii_lower(c);
                        }
                        out.push_back(pair<string, string>(string(std::string_view(name)), normalized(request, name)));
                    }
                }
                return out;
            }

            // An entry kept, in the place of the variant it matches; the
            // least recently used dropped past max_bytes
            void store(const tracked_ptr<CacheEntry>& e) {
                std::lock_guard<std::mutex> g(lock);
                auto& list = entries[e->url];
                for (size_t i = 0; i < list.size(); ++i) {
                    if (list[i]->vary == e->vary) {
                        bytes -= list[i]->size;
                        if (list[i]->file != e->file) {
                            drop_file(*list[i]);   // the new entry's files have the same name: written over already
                        }
                        list.erase(list.begin() + std::ptrdiff_t(i));
                        break;
                    }
                }
                e->used = ++clock;
                list.push_back(e);
                bytes += e->size;
                while (bytes > max_bytes) {
                    tracked_ptr<CacheEntry> oldest;
                    for (auto& [url, l] : entries) {
                        for (auto& x : l) {
                            if (!oldest || x->used < oldest->used) {
                                oldest = x;
                            }
                        }
                    }
                    if (!oldest || oldest == e) {
                        break;
                    }
                    erase_locked(oldest);
                }
            }

            void erase_locked(const tracked_ptr<CacheEntry>& e) {
                auto it = entries.find(e->url);
                if (it == entries.end()) {
                    return;
                }
                auto& l = it->second;
                for (size_t i = 0; i < l.size(); ++i) {
                    if (l[i] == e) {
                        bytes -= e->size;
                        drop_file(*e);
                        l.erase(l.begin() + std::ptrdiff_t(i));
                        break;
                    }
                }
                if (l.empty()) {
                    entries.erase(it);
                }
            }

            void erase_url(const string& url) {
                std::lock_guard<std::mutex> g(lock);
                auto it = entries.find(url);
                if (it == entries.end()) {
                    return;
                }
                for (auto& e : it->second) {
                    bytes -= e->size;
                    drop_file(*e);
                }
                entries.erase(it);
            }

            static void drop_file(const CacheEntry& e) {
                if (!e.file.empty()) {
                    (void)io::remove(e.file + ".head");
                    (void)io::remove(e.file + ".body");
                }
            }

            // --- the disk: an entry is two files in the directory, <key>.head
            // and <key>.body, the key the SHA-256 of the URL and the Vary
            // values (one file name a variant); the head is the entry's
            // fields, each a length, a colon, the bytes and a newline

            string file_of(const CacheEntry& e) const {
                std::string k(e.url.view());
                for (auto& [name, value] : e.vary) {
                    k += '\n';
                    k += name.view();
                    k += '\t';
                    k += value.view();
                }
                auto h = crypto::sha256::of(string(std::string_view(k)));
                return io::path::join(directory, encoding::hex::encode(h));
            }

            static void put(std::string& out, std::string_view v) {
                out += std::to_string(v.size());
                out += ':';
                out += v;
                out += '\n';
            }

            static void put(std::string& out, int64_t v) {
                put(out, std::string_view(std::to_string(v)));
            }

            static constexpr std::string_view HeadMagic = "sgcl-http-cache 1\n";

            std::string head_of(const CacheEntry& e) {
                std::string out(HeadMagic);
                std::lock_guard<std::mutex> g(lock);
                put(out, e.url.view());
                put(out, int64_t(e.vary.size()));
                for (auto& [name, value] : e.vary) {
                    put(out, name.view());
                    put(out, value.view());
                }
                put(out, int64_t(e.status));
                put(out, e.request_time);
                put(out, e.response_time);
                put(out, int64_t(e.uncompressed));
                put(out, int64_t(e.size));
                auto& fields = HeadersAccess::fields(e.fields);
                put(out, int64_t(fields.size()));
                for (auto& f : fields) {
                    put(out, f.first.view());
                    put(out, f.second.view());
                }
                return out;
            }

            static optional<std::string_view> take(std::string_view& in) noexcept {
                const size_t colon = in.find(':');
                if (colon == std::string_view::npos || colon == 0 || colon > 10) {
                    return nullopt;
                }
                uint64_t n = 0;
                for (char c : in.substr(0, colon)) {
                    if (c < '0' || c > '9') {
                        return nullopt;
                    }
                    n = n * 10 + uint64_t(c - '0');
                }
                if (in.size() - colon - 1 < n + 1 || in[colon + 1 + n] != '\n') {
                    return nullopt;
                }
                std::string_view v = in.substr(colon + 1, n);
                in.remove_prefix(colon + 2 + n);
                return v;
            }

            static optional<int64_t> take_number(std::string_view& in) noexcept {
                auto v = take(in);
                if (!v || v->empty() || v->size() > 19) {
                    return nullopt;
                }
                int64_t n = 0;
                bool negative = (*v)[0] == '-';
                for (char c : v->substr(negative ? 1 : 0)) {
                    if (c < '0' || c > '9') {
                        return nullopt;
                    }
                    n = n * 10 + (c - '0');
                }
                return negative ? -n : n;
            }

            // An entry of a head file; null for one that does not read back
            static tracked_ptr<CacheEntry> entry_of(std::string_view in) {
                if (in.substr(0, HeadMagic.size()) != HeadMagic) {
                    return tracked_ptr<CacheEntry>();
                }
                in.remove_prefix(HeadMagic.size());
                tracked_ptr e = make_tracked<CacheEntry>();
                auto url = take(in);
                auto nvary = take_number(in);
                if (!url || !nvary || *nvary < 0 || *nvary > 64) {
                    return tracked_ptr<CacheEntry>();
                }
                e->url = string(*url);
                for (int64_t i = 0; i < *nvary; ++i) {
                    auto name = take(in);
                    auto value = take(in);
                    if (!name || !value) {
                        return tracked_ptr<CacheEntry>();
                    }
                    e->vary.push_back(pair<string, string>(string(*name), string(*value)));
                }
                auto status = take_number(in);
                auto request_time = take_number(in);
                auto response_time = take_number(in);
                auto uncompressed = take_number(in);
                auto size = take_number(in);
                auto nfields = take_number(in);
                if (!status || *status < 100 || *status > 999 || !request_time || !response_time || !uncompressed || !size || *size < 0 ||
                    !nfields || *nfields < 0 || *nfields > 10000) {
                    return tracked_ptr<CacheEntry>();
                }
                e->status = int(*status);
                e->request_time = *request_time;
                e->response_time = *response_time;
                e->uncompressed = *uncompressed != 0;
                e->size = uint64_t(*size);
                for (int64_t i = 0; i < *nfields; ++i) {
                    auto name = take(in);
                    auto value = take(in);
                    if (!name || !value) {
                        return tracked_ptr<CacheEntry>();
                    }
                    e->fields.add(string(*name), string(*value));
                }
                return in.empty() ? e : tracked_ptr<CacheEntry>();
            }

            // The entries the directory holds, read at the cache's start: the
            // heads that read back and whose body has its size; the rest
            // (a write broken off, another program's file) removed
            void load() {
                auto list = io::read_dir(directory);
                if (!list) {
                    return;
                }
                vector<tracked_ptr<CacheEntry>> found;
                for (auto& d : *list) {
                    const std::string_view name = d.name.view();
                    if (name.size() <= 5 || name.substr(name.size() - 5) != ".head") {
                        if (name.size() > 4 && name.substr(name.size() - 4) == ".tmp") {
                            (void)io::remove(d.path);   // a write broken off
                        }
                        continue;
                    }
                    const string base = string(d.path.view().substr(0, d.path.size() - 5));
                    auto head = io::read_file(d.path);
                    tracked_ptr<CacheEntry> e;
                    if (head) {
                        e = entry_of(std::string_view(reinterpret_cast<const char*>(head->data()), head->size()));
                    }
                    auto body = io::stat(base + ".body");
                    if (!e || !body || body->size != e->size || file_of(*e) != base) {
                        (void)io::remove(d.path);
                        (void)io::remove(base + ".body");
                        continue;
                    }
                    e->file = base;
                    found.push_back(e);
                }
                std::sort(found.begin(), found.end(), [](const tracked_ptr<CacheEntry>& a, const tracked_ptr<CacheEntry>& b) {
                    return a->response_time < b->response_time;
                });
                for (auto& e : found) {
                    store(e);   // the oldest first: the least recently used
                }
            }
        };
    }

    // A private cache of a client's responses (RFC 9111): the client's
    // member `cache`, which every GET (and HEAD) of the client goes through.
    // A fresh stored response is served with no request sent; a stale one
    // is asked again with its validators (If-None-Match, If-Modified-Since)
    // and a 304 serves it refreshed; stale-while-revalidate serves it at once
    // and revalidates in the background, stale-if-error serves it when the
    // server fails. A handle of one word: copies share the entries, and any
    // number of clients may share one cache
    class cache {
    public:
        struct options {
            uint64_t max_bytes = uint64_t(64) << 20;        // the bodies kept together; the least recently used dropped past it
            uint64_t max_entry_bytes = uint64_t(8) << 20;   // a larger body is not stored
            double heuristic = 0.1;                         // a response without explicit freshness: this fraction of the time since its Last-Modified...
            duration heuristic_max = std::chrono::hours(24);   // ...at most this long (RFC 9111 §4.2.2)
        };

        // In memory
        cache()
        : cache(options()) {
        }

        explicit cache(const options& o)
        : _s(make_tracked<detail::CacheState>()) {
            _s->max_bytes = o.max_bytes;
            _s->max_entry_bytes = o.max_entry_bytes;
            _s->heuristic = o.heuristic;
            _s->heuristic_max = o.heuristic_max.nanoseconds() / 1000000000;
        }

        // In a directory, made when it is missing: the entries a cache of the
        // directory kept before are taken (every program that opens it
        // shares nothing at run time: one cache of a directory at a time)
        static expected<cache, io::error> on_disk(const string& directory) {
            return on_disk(directory, options());
        }

        static expected<cache, io::error> on_disk(const string& directory, const options& o) {
            if (auto made = io::mkdir_all(directory); !made) {
                return unexpected(made.error());
            }
            cache c(o);
            c._s->directory = directory;
            c._s->load();
            return c;
        }

        // How many responses the cache holds, and their bodies' bytes
        size_t size() const {
            std::lock_guard<std::mutex> g(_s->lock);
            size_t n = 0;
            for (auto& [url, l] : _s->entries) {
                n += l.size();
            }
            return n;
        }

        uint64_t bytes() const {
            std::lock_guard<std::mutex> g(_s->lock);
            return _s->bytes;
        }

        // Every entry dropped
        void clear() const {
            std::lock_guard<std::mutex> g(_s->lock);
            for (auto& [url, l] : _s->entries) {
                for (auto& e : l) {
                    detail::CacheState::drop_file(*e);
                }
            }
            _s->entries = map<string, vector<tracked_ptr<detail::CacheEntry>>>();
            _s->bytes = 0;
        }

        // The entries of a URL dropped (every variant)
        void erase(const string& url) const {
            _s->erase_url(url);
        }

    private:
        friend struct detail::CacheAccess;
        tracked_ptr<detail::CacheState> _s;
    };

    namespace detail {
        struct CacheAccess {
            SGCL_INLINE_HOT static const tracked_ptr<CacheState>& state(const cache& c) noexcept {
                return c._s;
            }
        };
    }
}
