//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// The encoding module: formats as types named after them, in
// sgcl::encoding (base64, base32, hex, ascii85, pem, big_endian,
// little_endian, varint, json, csv, xml), one error type for all of
// them, and one description of a program's types for all of them
// (fields.h).
#include "error.h"
#include "fields.h"
#include "ascii85.h"
#include "base32.h"
#include "base64.h"
#include "binary.h"
#include "csv.h"
#include "hex.h"
#include "json.h"
#include "pem.h"
#include "xml.h"
