//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// SMTP in sgcl::net::smtp: a client (send in one line, a session for
// several messages, delivery to the recipients' exchangers) and a server
// with a handler per message, on the messages of encoding::email
#include "envelope.h"
#include "client.h"
#include "server.h"
