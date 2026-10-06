//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// JSON-RPC 2.0 both ways: methods served over a stream, a WebSocket or
// HTTP, calls made over the same (net::jsonrpc)
#include "client.h"
#include "error.h"
#include "methods.h"
#include "peer.h"
