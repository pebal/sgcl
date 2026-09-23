//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// txt: what a human expects of text and a byte does not give — the
// properties of a code point, the boundaries between graphemes, words and
// lines, normalization, the full case mappings, collation and the
// encodings. core stays underneath it (utf8, unicode, runes), and this
// module only adds.
#include "properties.h"
#include "bidi.h"
#include "case.h"
#include "collate.h"
#include "encoding.h"
#include "format.h"
#include "normalize.h"
#include "search.h"
#include "segment.h"
