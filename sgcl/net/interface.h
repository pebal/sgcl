//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/sockaddr.h"
#include "error.h"
#include "ip.h"
#include "../core/aliases.h"
#include "../core/string.h"
#include "../core/vector.h"

#include <cerrno>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <string_view>
#include <sys/socket.h>

namespace sgcl::net {
    namespace detail { using namespace sgcl::detail; }

    // A network interface of this machine, as getifaddrs gives it: its
    // name ("en0", "lo0"), its index (if_nametoindex: the zone of an IPv6
    // address on it, the interface a multicast group is joined on), its
    // addresses with their prefix lengths, and whether it is up, a
    // loopback, able to multicast. Go's net.Interface with its Addrs. A
    // plain struct; a snapshot, not a handle to the interface.
    struct network_interface {
        string name;
        uint32_t index = 0;
        vector<ip_network> addresses;
        bool up = false;
        bool loopback = false;
        bool multicast = false;
    };

    namespace detail {
        // The prefix length of a netmask of `n` bytes: its leading ones
        SGCL_INLINE_HOT int mask_bits(const uint8_t* m, size_t n) noexcept {
            int bits = 0;
            for (size_t i = 0; i < n; ++i) {
                uint8_t b = m[i];
                while (b & 0x80) {
                    ++bits;
                    b = uint8_t(b << 1);
                }
                if (m[i] != 0xFF) {
                    break;
                }
            }
            return bits;
        }

        // The interfaces of getifaddrs, one entry per name in the list's
        // order (getifaddrs gives one per address, and one per link)
        inline expected<vector<network_interface>, io::error> list_interfaces() noexcept {
            ifaddrs* list = nullptr;
            if (::getifaddrs(&list) != 0) {
                return fail(system_error(errno, "interfaces"));
            }
            vector<network_interface> out;
            for (ifaddrs* p = list; p; p = p->ifa_next) {
                if (!p->ifa_name) {
                    continue;
                }
                std::string_view name(p->ifa_name);
                size_t at = out.size();
                for (size_t i = 0; i < out.size(); ++i) {
                    if (out[i].name.view() == name) {
                        at = i;
                        break;
                    }
                }
                if (at == out.size()) {
                    network_interface ifi;
                    ifi.name = string(name);
                    ifi.index = ::if_nametoindex(p->ifa_name);
                    ifi.up = (p->ifa_flags & IFF_UP) != 0;
                    ifi.loopback = (p->ifa_flags & IFF_LOOPBACK) != 0;
                    ifi.multicast = (p->ifa_flags & IFF_MULTICAST) != 0;
                    out.push_back(std::move(ifi));
                }
                const sockaddr* a = p->ifa_addr;
                if (!a || (a->sa_family != AF_INET && a->sa_family != AF_INET6)) {
                    continue;
                }
                ip_address address = from_sockaddr(a).address();
                int bits = a->sa_family == AF_INET ? 32 : 128;
                if (const sockaddr* m = p->ifa_netmask) {
                    if (a->sa_family == AF_INET) {
                        auto* m4 = reinterpret_cast<const sockaddr_in*>(m);
                        bits = mask_bits(reinterpret_cast<const uint8_t*>(&m4->sin_addr), 4);
                    } else {
                        auto* m6 = reinterpret_cast<const sockaddr_in6*>(m);
                        bits = mask_bits(reinterpret_cast<const uint8_t*>(&m6->sin6_addr), 16);
                    }
                }
                out[at].addresses.push_back(ip_network(address, bits));
            }
            ::freeifaddrs(list);
            return out;
        }

        // The interface of `index`, or not found
        inline expected<network_interface, io::error> interface_at(uint32_t index) noexcept {
            auto all = list_interfaces();
            if (!all) {
                return fail(all);
            }
            for (auto& i : *all) {
                if (i.index == index && index != 0) {
                    return i;
                }
            }
            return fail(system_error(ENXIO, "interface", to_string(index)));
        }
    }

    // The interfaces of this machine, each once, in the system's order:
    // Go's net.Interfaces with each one's Addrs
    inline expected<vector<network_interface>, io::error> interfaces() noexcept {
        return detail::list_interfaces();
    }

    // The interface of that name ("lo0", "eth0"): Go's
    // net.InterfaceByName; ENXIO when there is none
    inline expected<network_interface, io::error> interface_by_name(const string& name) noexcept {
        auto all = detail::list_interfaces();
        if (!all) {
            return detail::fail(all);
        }
        for (auto& i : *all) {
            if (i.name == name) {
                return i;
            }
        }
        return detail::fail(detail::system_error(ENXIO, "interface", name));
    }

    // The interface of that index: Go's net.InterfaceByIndex; ENXIO when
    // there is none (0 is none)
    inline expected<network_interface, io::error> interface_by_index(uint32_t index) noexcept {
        return detail::interface_at(index);
    }
}
