//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../../encoding/detail/media_types.h"

// The Content-Type of a file by its name: the file server's (serve.h) and
// a file part's of a form (form.h); the table is encoding's, shared with
// a mail's attachments (encoding/email.h)
namespace sgcl::net::http::detail {
    using sgcl::encoding::detail::content_type_of;
}
