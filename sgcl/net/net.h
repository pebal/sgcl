//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// The net module, stages 1a and 1b: addresses (ip_address, ip_network,
// net::endpoint), connections (net::connection, net::listener, net::udp::socket),
// the protocols that make them (tcp, udp, unix_domain), names (dns) and
// URLs (url, query_params). HTTP is sgcl/net/http/http.h.
#include "connection.h"
#include "dns.h"
#include "error.h"
#include "ip.h"
#include "socket.h"
#include "url.h"
