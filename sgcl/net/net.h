//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// The net module: addresses (ip_address, ip_network, net::endpoint),
// connections (net::connection, net::listener, net::udp::socket), the
// protocols that make them (tcp, udp, unix_domain, socks5 through a
// proxy), names (dns), URLs (url, query_params) and the machine's
// interfaces (interfaces). TLS 1.3 is sgcl/net/tls.h, HTTP
// sgcl/net/http/http.h.
#include "connection.h"
#include "dns.h"
#include "error.h"
#include "interface.h"
#include "ip.h"
#include "mdns.h"
#include "socket.h"
#include "socks5.h"
#include "url.h"
