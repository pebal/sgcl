//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// ACME of the net module (RFC 8555): net::acme::client, the protocol from
// the client's side; net::acme::manager, certificates obtained and renewed
// by themselves for a TLS server; net::acme::test_server, a CA on the
// loopback for tests; the account's key, the objects of the protocol, the
// errors of the acme category.
#include "error.h"
#include "key.h"
#include "types.h"
#include "client.h"
#include "manager.h"
#include "test_server.h"
