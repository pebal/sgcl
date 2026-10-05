//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// HTTP of the net module, 1.1 and 2, over TCP or TLS (https://):
// net::http::client with its pool, net::http::server with its routes,
// the messages (request, response, response_writer), headers, status,
// cookie, cookie_jar with the public suffix list, download, the client's
// proxy, multipart/form-data (form, multipart_reader), WebSocket,
// Server-Sent Events, a reverse proxy (reverse_proxy), the test utilities
// (test_server, response_recorder, test_request); the parsers under
// them are in detail/.
#include "download.h"
#include "events.h"
#include "client.h"
#include "cookie.h"
#include "cookie_jar.h"
#include "form.h"
#include "headers.h"
#include "multipart.h"
#include "proxy.h"
#include "public_suffix.h"
#include "request.h"
#include "response.h"
#include "response_writer.h"
#include "reverse_proxy.h"
#include "server.h"
#include "serve.h"
#include "status.h"
#include "test.h"
#include "websocket.h"
