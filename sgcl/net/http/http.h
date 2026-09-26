//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// HTTP/1.1 of the net module (stage 1c): net::http::client with its pool,
// net::http::server with its routes, the messages (request, response,
// response_writer), headers, status, cookie; the parser under them is
// detail/parser.h. TLS, https:// and HTTP/2 come with the next stage.
#include "client.h"
#include "cookie.h"
#include "headers.h"
#include "request.h"
#include "response.h"
#include "response_writer.h"
#include "server.h"
#include "status.h"
