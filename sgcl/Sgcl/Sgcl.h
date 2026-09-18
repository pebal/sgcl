//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The Sgcl interface: the library behind an object-oriented face,
// PascalCase types and methods, a class per type with the one object
// inside (Inner()) and the same semantics, cost and rules. Values with
// pointers: List<T> is a value, a Ptr<List<T>> is shared. Nothing is
// checked that the library does not check.
#pragma once

#include "Async/Async.h"
#include "Concurrent/Concurrent.h"
#include "Containers/Containers.h"
#include "Core/Core.h"
