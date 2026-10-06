//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// txt: what a human expects of text and a byte does not give — the
// properties of a code point, the boundaries between graphemes, words and
// lines, normalization, the full case mappings, collation, the
// encodings, and the locale data of CLDR: numbers, currencies, plurals,
// lists and relative time (dates and times in a locale are time's).
// core stays underneath it (utf8, unicode, runes), and this module only
// adds.
#include "properties.h"
#include "bidi.h"
#include "case.h"
#include "catalog.h"
#include "collate.h"
#include "currency.h"
#include "diff.h"
#include "distance.h"
#include "encoding.h"
#include "format.h"
#include "html.h"
#include "html_stencil.h"
#include "identifier.h"
#include "idna.h"
#include "list_format.h"
#include "markdown.h"
#include "locale.h"
#include "message_format.h"
#include "number.h"
#include "normalize.h"
#include "percent.h"
#include "plural.h"
#include "regex.h"
#include "relative_time.h"
#include "search.h"
#include "segment.h"
#include "stencil.h"
#include "tab_writer.h"
