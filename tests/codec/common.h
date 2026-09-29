//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the tests of the codec module share: the corpora in
// ~/Programming/oracles (PngSuite, libjpeg-turbo's and giflib's test
// images, libwebp-test-data), read whole; a test whose corpus is missing
// is skipped with the path it looked for.
#pragma once

#include "tests/types.h"
#include "sgcl/codec/codec.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace codec_test {
    // A directory of the temporary one for this process's files: `name`
    // with the process id, so that runs from several trees at once (or a
    // test program and its portable twin) neither write over each other's
    // files nor build an oracle into the same path
    inline std::filesystem::path scratch_path(const std::string& name) {
#if defined(_WIN32)
        const long pid = long(_getpid());
#else
        const long pid = long(getpid());
#endif
        auto dir = std::filesystem::temp_directory_path() / (name + "_" + std::to_string(pid));
        std::filesystem::create_directories(dir);
        return dir;
    }

    inline std::string oracle_path(const std::string& relative) {
        const char* home = std::getenv("HOME");
        return std::string(home ? home : "") + "/Programming/oracles/" + relative;
    }

    // The file's bytes, nullopt when it is not there
    inline std::optional<std::string> read_oracle(const std::string& relative) {
        std::ifstream is(oracle_path(relative), std::ios::binary);
        if (!is) {
            return std::nullopt;
        }
        std::stringstream ss;
        ss << is.rdbuf();
        return ss.str();
    }

    inline sgcl::slice<const std::byte> bytes(const std::string& s) {
        return sgcl::slice<const std::byte>(reinterpret_cast<const std::byte*>(s.data()), s.size());
    }
}

// The file of a corpus, or the test skipped with the path it looked for
#define CODEC_ORACLE(var, relative)                                                   \
    auto var##_opt = codec_test::read_oracle(relative);                               \
    if (!var##_opt) {                                                                 \
        GTEST_SKIP() << "no " << codec_test::oracle_path(relative);                   \
    }                                                                                 \
    const std::string& var = *var##_opt
