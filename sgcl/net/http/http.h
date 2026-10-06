//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// HTTP of the net module, 1.1 and 2, over TCP or TLS (https://):
// net::http::client with its pool, net::http::server with its routes,
// the messages (request, response, response_writer), headers, status,
// cookie, cookie_jar with the public suffix list, download (continued
// with Range and If-Range), files and content answered with validators,
// conditional requests and ranges (serve.h), the client's proxy, multipart/form-data (form, multipart_reader), WebSocket,
// Server-Sent Events, the middlewares (middleware.h: cors, recovery,
// request_log, body_limit, rate_limit; session.h: sessions; csrf.h; auth.h: basic_auth,
// digest_auth, which the client answers; compression.h: the responses
// compressed, which the client decodes), the client's cache (cache.h), a reverse proxy (reverse_proxy), the test utilities
// (test_server, response_recorder, test_request); the parsers under
// them are in detail/.
#include "auth.h"
#include "cache.h"
#include "download.h"
#include "events.h"
#include "client.h"
#include "compression.h"
#include "cookie.h"
#include "cookie_jar.h"
#include "csrf.h"
#include "form.h"
#include "headers.h"
#include "middleware.h"
#include "multipart.h"
#include "proxy.h"
#include "public_suffix.h"
#include "request.h"
#include "response.h"
#include "response_writer.h"
#include "reverse_proxy.h"
#include "server.h"
#include "serve.h"
#include "session.h"
#include "status.h"
#include "test.h"
#include "websocket.h"
