//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// HTTP of the net module, 1.1 and 2, over TCP or TLS (https://):
// net::http::client with its pool, net::http::server with its routes,
// the messages (request, response, response_writer), headers, status,
// cookie, download; the parsers under them are in detail/.
#include "download.h"
#include "client.h"
#include "cookie.h"
#include "headers.h"
#include "request.h"
#include "response.h"
#include "response_writer.h"
#include "server.h"
#include "serve.h"
#include "status.h"
