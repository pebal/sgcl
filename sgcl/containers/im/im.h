//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The immutable containers, a family of the containers module in a
// namespace of their own: sgcl::im::vector, list, map
// and set, every operation of which returns a new container that shares
// all but the path it changed with the old one, which stays as it was.
#pragma once

#include "list.h"
#include "map.h"
#include "set.h"
#include "vector.h"
