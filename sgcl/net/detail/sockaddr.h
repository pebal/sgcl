//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../ip.h"
#include "../../core/aliases.h"
#include "../../core/string.h"

#include <arpa/inet.h>
#include <cstring>
#include <net/if.h>
#include <netinet/in.h>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>

namespace sgcl::net::detail {
    using namespace sgcl::detail;
    // A socket address of any family, as the calls take and give it
    struct SockAddr {
        sockaddr_storage storage = {};
        socklen_t size = sizeof(sockaddr_storage);

        sockaddr* get() noexcept {
            return reinterpret_cast<sockaddr*>(&storage);
        }

        const sockaddr* get() const noexcept {
            return reinterpret_cast<const sockaddr*>(&storage);
        }

        int family() const noexcept {
            return storage.ss_family;
        }
    };

    // The interface index of a zone: a number as it stands ("%4"), a name
    // through if_nametoindex ("%en0"); 0 when there is none, or no such
    // interface
    inline uint32_t scope_of(const ip_address& a) {
        if (!a.has_zone()) {
            return 0;
        }
        auto zone = a.zone();
        uint64_t n = 0;
        bool digits = true;
        for (char c : std::string_view(zone.data(), zone.size())) {
            if (c < '0' || c > '9') {
                digits = false;
                break;
            }
            n = n * 10 + uint64_t(c - '0');
            if (n > 0xffffffffu) {
                return 0;   // no index is that large: no interface, not another one's by wrap-around
            }
        }
        return digits ? uint32_t(n) : ::if_nametoindex(zone.c_str());
    }

    // The endpoint as the address of a socket of the family: an IPv4
    // address on an IPv6 socket as the mapped one (a dual-stack socket
    // reaches IPv4 so); false for an IPv6 address on an IPv4 socket, and
    // for a zone that names no interface
    inline bool to_sockaddr(const endpoint& e, int family, SockAddr& out) {
        out = SockAddr();
        auto a = e.address();
        if (family == AF_INET) {
            a = a.unmap();
            if (!a.is_v4()) {
                return false;
            }
            auto* s = reinterpret_cast<sockaddr_in*>(&out.storage);
            s->sin_family = AF_INET;
#ifdef __APPLE__
            s->sin_len = sizeof(sockaddr_in);
#endif
            s->sin_port = htons(e.port());
            auto b = a.bytes();
            std::memcpy(&s->sin_addr, &b[12], 4);
            out.size = sizeof(sockaddr_in);
            return true;
        }
        auto* s = reinterpret_cast<sockaddr_in6*>(&out.storage);
        s->sin6_family = AF_INET6;
#ifdef __APPLE__
        s->sin6_len = sizeof(sockaddr_in6);
#endif
        s->sin6_port = htons(e.port());
        auto b = a.bytes();   // an IPv4 address comes out mapped
        std::memcpy(&s->sin6_addr, &b[0], 16);
        if (a.has_zone()) {
            s->sin6_scope_id = scope_of(a);
            if (s->sin6_scope_id == 0) {
                return false;
            }
        }
        out.size = sizeof(sockaddr_in6);
        return true;
    }

    // The address of an IP socket as an endpoint: an IPv4-mapped address
    // (what a dual-stack socket reports for an IPv4 peer) as the IPv4 one,
    // a scope as the zone (the interface's name, or its number when it has
    // none); the empty endpoint for any other family
    inline endpoint from_sockaddr(const sockaddr* sa) {
        if (sa->sa_family == AF_INET) {
            auto* s = reinterpret_cast<const sockaddr_in*>(sa);
            auto* b = reinterpret_cast<const uint8_t*>(&s->sin_addr);
            return endpoint(ip_address::v4(b[0], b[1], b[2], b[3]), ntohs(s->sin_port));
        }
        if (sa->sa_family == AF_INET6) {
            auto* s = reinterpret_cast<const sockaddr_in6*>(sa);
            array<uint8_t, 16> b;
            std::memcpy(b.data(), &s->sin6_addr, 16);
            auto a = ip_address::v6(b).unmap();
            if (s->sin6_scope_id != 0 && a.is_v6()) {
                char name[IF_NAMESIZE] = {};
                if (::if_indextoname(s->sin6_scope_id, name)) {
                    a = a.with_zone(string(name));
                } else {
                    a = a.with_zone(sgcl::to_string(s->sin6_scope_id));
                }
            }
            return endpoint(a, ntohs(s->sin6_port));
        }
        return endpoint();
    }

    // The address of a unix socket at the path; false for a path longer
    // than sun_path holds (104 bytes on macOS, 108 on Linux, the
    // terminator included) or with a NUL in it
    inline bool unix_sockaddr(const string& path, SockAddr& out) noexcept {
        out = SockAddr();
        auto* s = reinterpret_cast<sockaddr_un*>(&out.storage);
        std::string_view p(path.data(), path.size());
        if (p.empty() || p.size() >= sizeof(s->sun_path) || p.find('\0') != std::string_view::npos) {
            return false;
        }
        s->sun_family = AF_UNIX;
        std::memcpy(s->sun_path, p.data(), p.size());
#ifdef __APPLE__
        s->sun_len = uint8_t(offsetof(sockaddr_un, sun_path) + p.size() + 1);
#endif
        out.size = socklen_t(offsetof(sockaddr_un, sun_path) + p.size() + 1);
        return true;
    }

    // The endpoint the socket is bound to (getsockname); empty for a unix socket
    inline endpoint local_of(int fd) {
        SockAddr a;
        if (::getsockname(fd, a.get(), &a.size) != 0) {
            return endpoint();
        }
        return from_sockaddr(a.get());
    }

    inline endpoint remote_of(int fd) {
        SockAddr a;
        if (::getpeername(fd, a.get(), &a.size) != 0) {
            return endpoint();
        }
        return from_sockaddr(a.get());
    }
}
