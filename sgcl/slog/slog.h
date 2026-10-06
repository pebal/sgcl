//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// sgcl::slog: structured logging, as Go's log/slog. A record is a level, a
// message and attributes, key-value pairs checked by the compiler; it is
// written as text or JSON, byte for byte as slog writes it, or given to a
// handler of the program:
//
//     slog::info("server started", "port", 8080, "tls", true);
//
//     slog::logger log(slog::options{.out = file, .level = slog::level::debug, .json = true});
//     log.with("service", "api").warn("slow request", "path", path, "took", took);
#include "handler.h"
#include "level.h"
#include "collector_log.h"
#include "logger.h"
#include "memory.h"
#include "record.h"
#include "rotating_file.h"
#include "syslog.h"
