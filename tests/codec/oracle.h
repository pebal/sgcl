//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The oracles of the codec tests: tools/codec_oracle.c (the reference C
// libraries: libpng from Homebrew, SGCL_LIBPNG_ROOT to point elsewhere) and
// tools/codec_oracle.go (Go's image packages), each built once per run into
// the temporary directory and run per file. Both write the same form: "W H
// D", then the pixels RGBA at D bits a channel, 16-bit ones big-endian. A
// test whose oracle cannot be built is skipped.
#pragma once

#include "common.h"
#include "tests/source_root.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace codec_test {
    inline std::filesystem::path repository_root() {
        return source_root();
    }

    inline std::string libpng_root() {
        if (const char* r = std::getenv("SGCL_LIBPNG_ROOT")) {
            return r;
        }
        for (const char* r : {"/opt/homebrew/opt/libpng", "/usr/local/opt/libpng"}) {
            if (std::filesystem::exists(std::string(r) + "/include/png.h")) {
                return r;
            }
        }
        return "";
    }

    // libjpeg-turbo from Homebrew, SGCL_LIBJPEG_ROOT to point elsewhere
    inline std::string libjpeg_root() {
        if (const char* r = std::getenv("SGCL_LIBJPEG_ROOT")) {
            return r;
        }
        for (const char* r : {"/opt/homebrew/opt/jpeg-turbo", "/usr/local/opt/jpeg-turbo"}) {
            if (std::filesystem::exists(std::string(r) + "/include/jpeglib.h")) {
                return r;
            }
        }
        return "";
    }

    // giflib from Homebrew, SGCL_GIFLIB_ROOT to point elsewhere
    inline std::string giflib_root() {
        if (const char* r = std::getenv("SGCL_GIFLIB_ROOT")) {
            return r;
        }
        for (const char* r : {"/opt/homebrew/opt/giflib", "/usr/local/opt/giflib"}) {
            if (std::filesystem::exists(std::string(r) + "/include/gif_lib.h")) {
                return r;
            }
        }
        return "";
    }

    // libwebp and libwebpdemux from Homebrew, SGCL_LIBWEBP_ROOT to point
    // elsewhere
    inline std::string libwebp_root() {
        if (const char* r = std::getenv("SGCL_LIBWEBP_ROOT")) {
            return r;
        }
        for (const char* r : {"/opt/homebrew/opt/webp", "/usr/local/opt/webp"}) {
            if (std::filesystem::exists(std::string(r) + "/include/webp/demux.h")) {
                return r;
            }
        }
        return "";
    }

    // The C oracle's path, "" when it cannot be built
    inline const std::string& c_oracle() {
        static std::string path = [] {
            const std::string png = libpng_root();
            if (png.empty() || std::system("command -v cc > /dev/null 2>&1") != 0) {
                return std::string();
            }
            auto src = repository_root() / "tools" / "codec_oracle.c";
            auto out = scratch_path("sgcl_codec_oracles") / "oracle_c";
            std::string base = "cc -O2 -o '" + out.string() + "' '" + src.string() + "' -I'" + png + "/include' -L'" + png + "/lib' -lpng";
            // with libjpeg and giflib when there are (their modes), else without
            const std::string jpeg = libjpeg_root(), gif = giflib_root();
            const std::string with_jpeg = jpeg.empty() ? " -DCODEC_ORACLE_NO_JPEG" : " -I'" + jpeg + "/include' -L'" + jpeg + "/lib' -ljpeg";
            const std::string with_gif = gif.empty() ? " -DCODEC_ORACLE_NO_GIF" : " -I'" + gif + "/include' -L'" + gif + "/lib' -lgif";
            const std::string webp = libwebp_root();
            const std::string with_webp = webp.empty() ? " -DCODEC_ORACLE_NO_WEBP" : " -I'" + webp + "/include' -L'" + webp + "/lib' -lwebp -lwebpdemux";
            if (std::system((base + with_jpeg + with_gif + with_webp + " 2>&1").c_str()) == 0) {
                return out.string();
            }
            if (std::system((base + " -DCODEC_ORACLE_NO_JPEG -DCODEC_ORACLE_NO_GIF -DCODEC_ORACLE_NO_WEBP 2>&1").c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }

    // The Go oracle's path, "" when it cannot be built
    inline const std::string& go_oracle() {
        static std::string path = [] {
            if (std::system("command -v go > /dev/null 2>&1") != 0) {
                return std::string();
            }
            auto src = repository_root() / "tools" / "codec_oracle.go";
            auto out = scratch_path("sgcl_codec_oracles") / "oracle_go";
            std::string cmd = "go build -o '" + out.string() + "' '" + src.string() + "' 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }

    // The Go oracle of WebP (tools/codec_oracle_webp.go, golang.org/x/image/
    // webp): built in a module of its own from the module cache in
    // ~/Programming/oracles/gomod, offline; "" when it cannot be built
    inline const std::string& go_webp_oracle() {
        static std::string path = [] {
            const char* home = std::getenv("HOME");
            if (!home || std::system("command -v go > /dev/null 2>&1") != 0) {
                return std::string();
            }
            const std::string cache = std::string(home) + "/Programming/oracles/gomod";
            if (!std::filesystem::is_directory(cache + "/golang.org/x/image@v0.46.0/webp")) {
                return std::string();
            }
            auto dir = scratch_path("sgcl_codec_oracle_webp_go");
            std::filesystem::copy_file(repository_root() / "tools" / "codec_oracle_webp.go", dir / "main.go",
                                       std::filesystem::copy_options::overwrite_existing);
            {
                std::ofstream mod(dir / "go.mod");
                mod << "module sgcl_codec_oracle_webp\n\ngo 1.26\n\nrequire golang.org/x/image v0.46.0\n";
            }
            auto out = dir / "codec_oracle_webp";
            const std::string cmd = "cd '" + dir.string() + "' && GOFLAGS=-mod=mod GOPROXY=off GOSUMDB=off GOMODCACHE='" + cache +
                                    "' go build -o '" + out.string() + "' . 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }

    // The HEIF oracle's path (ImageIO, macOS: tools/codec_oracle_heif.c),
    // "" elsewhere or when it cannot be built. Built for the tests' own
    // architecture: an x86-64 build under Rosetta runs x86-64 ImageIO, whose
    // HEVC and AV1 decoders give other pixels than arm64's
    inline const std::string& heif_oracle() {
        static std::string path = [] {
#if defined(__APPLE__)
            if (std::system("command -v cc > /dev/null 2>&1") != 0) {
                return std::string();
            }
#if defined(__x86_64__)
            const std::string arch = "x86_64";
#else
            const std::string arch = "arm64";
#endif
            auto src = repository_root() / "tools" / "codec_oracle_heif.c";
            auto out = scratch_path("sgcl_codec_oracles") / ("oracle_heif_" + arch);
            const std::string cmd = "cc -O2 -arch " + arch + " -o '" + out.string() + "' '" + src.string() +
                                    "' -framework ImageIO -framework CoreGraphics -framework CoreFoundation 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
#else
            return std::string();
#endif
        }();
        return path;
    }

    // What an oracle made of a file: the header line and the pixels, or
    // nullopt when it refused the file
    struct oracle_image {
        uint32_t width = 0;
        uint32_t height = 0;
        int depth = 0;
        std::string pixels;
    };

    inline std::optional<oracle_image> run_oracle(const std::string& exe, const std::string& mode, const std::string& file) {
        std::string cmd = "'" + exe + "' " + mode + " '" + file + "' 2>/dev/null";
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) {
            return std::nullopt;
        }
        std::string out;
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), p)) > 0) {
            out.append(buf, n);
        }
        if (pclose(p) != 0) {
            return std::nullopt;
        }
        oracle_image im;
        auto nl = out.find('\n');
        if (nl == std::string::npos || std::sscanf(out.c_str(), "%u %u %d", &im.width, &im.height, &im.depth) != 3) {
            return std::nullopt;
        }
        im.pixels = out.substr(nl + 1);
        return im;
    }

    // An image of the module in the oracles' form: RGBA at `depth` bits, a
    // 16-bit channel big-endian
    inline std::string oracle_form(const sgcl::codec::image& im, int depth) {
        using sgcl::codec::pixel_format;
        sgcl::codec::image rgba = im.convert(depth == 16 ? pixel_format::rgba16 : pixel_format::rgba8);
        auto px = rgba.pixels();
        std::string out(reinterpret_cast<const char*>(px.data()), px.size());
        if (depth == 16) {
            for (size_t i = 0; i + 1 < out.size(); i += 2) {
                uint16_t v;
                std::memcpy(&v, out.data() + i, 2);
                out[i] = char(v >> 8);
                out[i + 1] = char(v & 0xFF);
            }
        }
        return out;
    }
}
