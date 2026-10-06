//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// The encoding module: formats as types named after them, in
// sgcl::encoding (asn1, base64, base32, cbor, hex, ascii85, pem, big_endian,
// little_endian, varint, quoted_printable, json, csv, xml, email, uuid,
// msgpack, yaml), one
// error type for all of
// them, and one description of a program's types for all of them
// (fields.h).
#include "error.h"
#include "fields.h"
#include "ascii85.h"
#include "asn1.h"
#include "base32.h"
#include "base64.h"
#include "binary.h"
#include "cbor.h"
#include "content_line.h"
#include "csv.h"
#include "dotenv.h"
#include "email.h"
#include "hex.h"
#include "icalendar.h"
#include "ini.h"
#include "json.h"
#include "json_schema.h"
#include "msgpack.h"
#include "pem.h"
#include "quoted_printable.h"
#include "recurrence.h"
#include "toml.h"
#include "uuid.h"
#include "vcard.h"
#include "xml.h"
#include "yaml.h"
