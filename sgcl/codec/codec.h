//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// The codec module: images in memory (image, pixel_format) and the file
// formats PNG, JPEG, GIF and WebP, and HEIF and AVIF through the system
// (heif.h); load and save on files (files.h)
#include "decode.h"
#include "error.h"
#include "files.h"
#include "format.h"
#include "frames.h"
#include "gif.h"
#include "heif.h"
#include "image.h"
#include "jpeg.h"
#include "options.h"
#include "png.h"
#include "webp.h"
