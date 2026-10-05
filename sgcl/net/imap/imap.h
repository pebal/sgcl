//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// IMAP of the net module (RFC 9051, IMAP4rev2, with IMAP4rev1's peers):
// net::imap::client, net::imap::server over a backend (memory_backend,
// maildir_backend, or a program's own), the values of the protocol
// (sequence_set, message, envelope, body_structure, criteria ...); the
// parsers under them are in detail/.
#include "backend.h"
#include "client.h"
#include "criteria.h"
#include "error.h"
#include "maildir.h"
#include "server.h"
#include "types.h"
