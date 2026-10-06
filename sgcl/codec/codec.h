//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// The codec module: images in memory (image, pixel_format) and the file
// formats PNG, JPEG, GIF, WebP, BMP, TIFF, ICO, QOI and Netpbm, and HEIF
// and AVIF through the system (heif.h); load and save on files (files.h)
#include "bmp.h"
#include "decode.h"
#include "error.h"
#include "files.h"
#include "format.h"
#include "frames.h"
#include "gif.h"
#include "heif.h"
#include "ico.h"
#include "image.h"
#include "jpeg.h"
#include "jxl.h"
#include "metadata.h"
#include "options.h"
#include "png.h"
#include "qr.h"
#include "pnm.h"
#include "qoi.h"
#include "tiff.h"
#include "webp.h"
