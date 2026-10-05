//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "dns_message.h"
#include "../ip.h"
#include "../../core/detail/os.h"

#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

// /etc/resolv.conf, as resolv.conf(5) has it (and macOS's resolver(5)):
// what the stub resolver of net::dns takes from it
//
//   nameserver <address>          up to three (MAXNS), IPv4 or IPv6, a zone allowed
//   domain <name>                 the search list of that one name
//   search <name>...              the search list, up to six names (MAXDNSRCH)
//   port <n>                      the port of the servers (macOS), 53 by default
//   options ndots:n timeout:n attempts:n rotate use-vc
//
// ndots is capped at 15, timeout (seconds, per query) at 30 and at least 1,
// attempts at 5 and at least 1, as resolv.conf(5) caps them; a number
// that is not one reads as 0, as atoi reads it, and is then capped. The
// last of `domain` and `search` wins. A line that starts with '#' or ';' is
// a comment; a keyword it does not know is skipped. Without a search line
// the domain of the host's name is the search list (what follows the
// first dot of gethostname's), and without a nameserver line the server is
// the local machine's, 127.0.0.1 and ::1, as resolv.conf(5) says. Plain
// std types inside: the configuration is kept by a static, shared by every
// thread, and holds nothing of the collector's.
namespace sgcl::net::detail {
    struct ResolvConf {
        std::vector<endpoint> servers;
        std::vector<std::string> search;   // each rooted, "corp.example."
        int ndots = 1;
        int timeout = 5;                   // seconds, for one query to one server
        int attempts = 2;                  // the rounds over the servers
        bool rotate = false;               // the servers in turn, a lookup starting at the next
        bool tcp = false;                  // use-vc: every query over TCP
        bool default_servers = false;      // no nameserver line: the local machine's

        static constexpr size_t MaxServers = 3;
        static constexpr size_t MaxSearch = 6;
        static constexpr int MaxNdots = 15;
        static constexpr int MaxTimeout = 30;
        static constexpr int MaxAttempts = 5;
    };

    // The leading decimal digits of s, saturated; 0 when there are none
    SGCL_INLINE_HOT int resolv_number(std::string_view s) noexcept {
        long v = 0;
        for (char c : s) {
            if (c < '0' || c > '9') {
                break;
            }
            v = v * 10 + (c - '0');
            if (v > 1000000) {
                v = 1000000;
            }
        }
        return int(v);
    }

    SGCL_INLINE_HOT std::string resolv_rooted(std::string_view name) noexcept {
        std::string s(name);
        if (s.empty() || s.back() != '.') {
            s += '.';
        }
        return s;
    }

    // The text of a resolv.conf; `hostname` gives the search list when the
    // text has none
    inline ResolvConf parse_resolv_conf(std::string_view text, std::string_view hostname) noexcept {
        ResolvConf c;
        bool searched = false;
        int port = 53;
        std::vector<ip_address> addresses;
        while (!text.empty()) {
            size_t eol = text.find('\n');
            std::string_view line = text.substr(0, eol);
            text = eol == std::string_view::npos ? std::string_view() : text.substr(eol + 1);
            if (!line.empty() && (line[0] == '#' || line[0] == ';')) {
                continue;
            }
            std::string_view fields[16];
            size_t n = 0;
            size_t i = 0;
            while (i < line.size() && n < 16) {
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\f' || line[i] == '\v')) {
                    ++i;
                }
                size_t start = i;
                while (i < line.size() && !(line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\f' || line[i] == '\v')) {
                    ++i;
                }
                if (i > start) {
                    fields[n++] = line.substr(start, i - start);
                }
            }
            if (n == 0) {
                continue;
            }
            std::string_view key = fields[0];
            if (key == "nameserver") {
                if (n > 1 && addresses.size() < ResolvConf::MaxServers) {
                    if (auto a = IpText::parse(fields[1])) {
                        addresses.push_back(*a);
                    }
                }
            } else if (key == "domain") {
                if (n > 1) {
                    c.search.clear();
                    if (fields[1] != ".") {   // the root: no search, as in a search line
                        c.search.push_back(resolv_rooted(fields[1]));
                    }
                    searched = true;
                }
            } else if (key == "search") {
                c.search.clear();
                for (size_t k = 1; k < n && c.search.size() < ResolvConf::MaxSearch; ++k) {
                    if (fields[k] == ".") {
                        continue;
                    }
                    c.search.push_back(resolv_rooted(fields[k]));
                }
                searched = true;
            } else if (key == "port") {
                if (n > 1) {
                    int p = resolv_number(fields[1]);
                    if (p > 0 && p <= 65535) {
                        port = p;
                    }
                }
            } else if (key == "options") {
                for (size_t k = 1; k < n; ++k) {
                    std::string_view o = fields[k];
                    if (o.substr(0, 6) == "ndots:") {
                        int v = resolv_number(o.substr(6));
                        c.ndots = v > ResolvConf::MaxNdots ? ResolvConf::MaxNdots : v;
                    } else if (o.substr(0, 8) == "timeout:") {
                        int v = resolv_number(o.substr(8));
                        c.timeout = v < 1 ? 1 : v > ResolvConf::MaxTimeout ? ResolvConf::MaxTimeout : v;
                    } else if (o.substr(0, 9) == "attempts:") {
                        int v = resolv_number(o.substr(9));
                        c.attempts = v < 1 ? 1 : v > ResolvConf::MaxAttempts ? ResolvConf::MaxAttempts : v;
                    } else if (o == "rotate") {
                        c.rotate = true;
                    } else if (o == "use-vc" || o == "usevc" || o == "tcp") {
                        c.tcp = true;
                    }
                }
            }
        }
        for (auto& a : addresses) {
            c.servers.push_back(endpoint(a, uint16_t(port)));
        }
        if (c.servers.empty()) {
            c.servers.push_back(endpoint(ip_address::loopback_v4(), uint16_t(port)));
            c.servers.push_back(endpoint(ip_address::loopback_v6(), uint16_t(port)));
            c.default_servers = true;
        }
        if (!searched) {
            size_t dot = hostname.find('.');
            if (dot != std::string_view::npos && dot + 1 < hostname.size()) {
                std::string_view domain = hostname.substr(dot + 1);
                bool plain = true;
                for (char ch : domain) {
                    plain = plain && uint8_t(ch) > ' ';
                }
                if (plain && domain != ".") {   // a name of one field, as a search line's
                    c.search.push_back(resolv_rooted(domain));
                }
            }
        }
        return c;
    }

    // The order of the names a text stands for: -1 the name as given,
    // k >= 0 the name under the search list's k-th domain
    struct DnsSearchOrder {
        int order[ResolvConf::MaxSearch + 1];
        size_t count = 0;
    };

    inline DnsSearchOrder dns_search_order(const DnsNameText& info, const ResolvConf& conf) noexcept {
        DnsSearchOrder o;
        if (info.rooted) {
            o.order[o.count++] = -1;
            return o;
        }
        bool first = info.dots >= conf.ndots;
        if (first) {
            o.order[o.count++] = -1;
        }
        for (size_t k = 0; k < conf.search.size() && k < ResolvConf::MaxSearch; ++k) {
            o.order[o.count++] = int(k);
        }
        if (!first) {
            o.order[o.count++] = -1;
        }
        return o;
    }

    inline std::string host_name() noexcept {
        char name[256];
        if (::gethostname(name, sizeof(name)) != 0) {
            return std::string();
        }
        name[sizeof(name) - 1] = 0;
        return std::string(name);
    }

    // The file's text, at most 64 KiB of it; empty when it cannot be read
    inline std::string read_small_file(const std::string& path) noexcept {
        std::string text;
        int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            return text;
        }
        char buffer[4096];
        while (text.size() < 65536) {
            ssize_t n = ::read(fd, buffer, sizeof(buffer));
            if (n < 0 && errno == EINTR) {
                continue;
            }
            if (n <= 0) {
                break;
            }
            text.append(buffer, size_t(n));
        }
        ::close(fd);
        return text;
    }

    // The configuration of the file, read again when it changed: looked
    // at (stat) at most once in five seconds, as Go and glibc look
    class ResolvConfCache {
    public:
        std::shared_ptr<const ResolvConf> get() noexcept {
            std::lock_guard lock(_m);
            auto now = std::chrono::steady_clock::now();
            if (_conf && now - _checked < std::chrono::seconds(5)) {
                return _conf;
            }
            _checked = now;
            struct stat st = {};
            bool exists = ::stat(_path.c_str(), &st) == 0;
            Stamp stamp;
            if (exists) {
                stamp.size = int64_t(st.st_size);
                stamp.inode = uint64_t(st.st_ino);
#if defined(__APPLE__)
                stamp.mtime = int64_t(st.st_mtimespec.tv_sec) * 1000000000 + st.st_mtimespec.tv_nsec;
#else
                stamp.mtime = int64_t(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec;
#endif
            }
            if (_conf && stamp == _stamp) {
                return _conf;
            }
            _stamp = stamp;
            std::string text = exists ? read_small_file(_path) : std::string();
            _conf = std::make_shared<const ResolvConf>(parse_resolv_conf(text, host_name()));
            return _conf;
        }

        // For the tests: another file, read at the next get()
        void set_path(std::string path) noexcept {
            std::lock_guard lock(_m);
            _path = std::move(path);
            _conf.reset();
        }

        std::string path() noexcept {
            std::lock_guard lock(_m);
            return _path;
        }

    private:
        struct Stamp {
            int64_t size = -1;
            int64_t mtime = 0;
            uint64_t inode = 0;

            friend bool operator==(const Stamp&, const Stamp&) = default;
        };

        std::mutex _m;
        std::string _path = "/etc/resolv.conf";
        std::shared_ptr<const ResolvConf> _conf;
        std::chrono::steady_clock::time_point _checked;
        Stamp _stamp;
    };

    inline ResolvConfCache& resolv_conf_cache() noexcept {
        static ResolvConfCache cache;
        return cache;
    }
}
