//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "secret.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/string.h"
#include "../io/error.h"
#include "../io/file.h"

#include <cstddef>

namespace sgcl::crypto {
    // A file's bytes as a secret: a private key's PEM, a password, a key
    // file. The bytes go straight from the file into a secret_bytes, never
    // through managed memory (io::read_file gives a managed vector); every
    // block a growth leaves behind is zeroed. The file is read to its end
    // on this thread (a task's form, if one is asked for, would run it on
    // the blocking pool).
    inline expected<secret_bytes, io::error> read_secret(const string& path) {
        auto f = io::open(path);
        if (!f) {
            return unexpected(f.error());
        }
        secret_bytes out(secret_bytes::inline_capacity);
        size_t n = 0;
        for (;;) {
            if (n == out.size()) {
                out.resize(out.size() * 2);
            }
            auto r = f->read(out.as_slice().subslice(n));
            if (!r) {
                return unexpected(r.error());
            }
            if (*r == 0) {
                break;
            }
            n += *r;
        }
        out.resize(n);
        (void)f->close();
        return out;
    }
}
