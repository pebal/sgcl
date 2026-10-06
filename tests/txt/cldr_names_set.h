//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The display names the tests of sgcl/txt/names include: languages of each
// script and family, a variant (de-CH, which includes de) and a variant of a
// variant (es-MX, of es-419, of es). tools/cldr_driver.cpp built with
// -DSGCL_CLDR_NAMES includes the same, for the vectors.
#pragma once

#include "sgcl/txt/names/ar.h"
#include "sgcl/txt/names/de-CH.h"
#include "sgcl/txt/names/en-GB.h"
#include "sgcl/txt/names/es-MX.h"
#include "sgcl/txt/names/fr.h"
#include "sgcl/txt/names/hi.h"
#include "sgcl/txt/names/ja.h"
#include "sgcl/txt/names/pl.h"
#include "sgcl/txt/names/pt.h"
#include "sgcl/txt/names/ru.h"
#include "sgcl/txt/names/sr-Latn.h"
#include "sgcl/txt/names/zh-Hant.h"

inline constexpr const char* NamesLocales[] = {"ar", "de", "de-CH", "en", "en-GB", "es", "es-MX", "fr", "hi", "ja",
                                               "pl", "pt", "ru", "sr-Latn", "zh-Hant"};
