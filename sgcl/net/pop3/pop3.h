//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// POP3 in sgcl::net::pop3: a client (STLS, pop3s, AUTH PLAIN, APOP, UIDL,
// TOP, PIPELINING) and a server of the INBOX of an imap::backend
#include "error.h"
#include "types.h"
#include "client.h"
#include "server.h"
