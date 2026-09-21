//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <optional>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>

namespace sgcl {
    // The standard types that hold a tracked_ptr or a weak_ptr correctly
    // as they are, under the library's names so that the safe set is one
    // namespace: each keeps its value at a fixed offset of its own, one
    // value per place, so a pointer inside never shares its word with
    // data (README: Pointer maps). std::variant, std::any, std::function
    // and std::expected do not: variant.h, any.h, function.h and
    // expected.h are theirs. Nothing here for std::shared_ptr and
    // std::weak_ptr: a managed object is held by a tracked_ptr.
    using std::optional;
    using std::nullopt;
    using std::nullopt_t;
    using std::make_optional;
    using std::pair;
    using std::make_pair;
    using std::tuple;
    using std::make_tuple;
    using std::tie;
    using std::forward_as_tuple;
    using std::tuple_size;
    using std::tuple_size_v;
    using std::tuple_element;
    using std::tuple_element_t;

    // The error code of the standard library, under the library's name:
    // what an io::error carries (a value of errno in the system category,
    // or a code of a category of the library's own)
    using std::error_code;
    using std::error_category;
    using std::error_condition;

    // The current thread of the standard library, under the library's
    // name; the thread itself is the library's own (thread.h), its closure
    // in a managed node
    namespace this_thread = std::this_thread;   // yield, sleep_for, sleep_until, get_id
}
