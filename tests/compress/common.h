//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the tests of the compress module share: the corpus (Go's test data
// in ~/Programming/oracles/go-compress, made data of the lengths at the
// edges), and the system's zlib as the oracle both ways.
#pragma once

#include "tests/types.h"
#include "sgcl/compress/compress.h"

#include <zlib.h>

#include <unistd.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace compress_test {
    namespace compress = sgcl::compress;

    inline std::string oracle_path(const std::string& name) {
        return std::string(std::getenv("HOME")) + "/Programming/oracles/go-compress/" + name;
    }

    inline std::string read_oracle(const std::string& name) {
        std::ifstream is(oracle_path(name), std::ios::binary);
        std::stringstream ss;
        ss << is.rdbuf();
        return ss.str();
    }

    inline sgcl::slice<const std::byte> bytes(const std::string& s) {
        return sgcl::slice<const std::byte>(reinterpret_cast<const std::byte*>(s.data()), s.size());
    }

    inline std::string text(const sgcl::vector<std::byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    // A writer that takes `room` bytes and then fails: EIO the first time,
    // ENOSPC every time after, so that a failure handed on is told from a
    // new one; `calls` counts the writes tried
    class failing_after final : public sgcl::io::mixin::writer<failing_after> {
    public:
        using sgcl::io::mixin::writer<failing_after>::write;
        using sgcl::io::mixin::writer<failing_after>::async_write;

        explicit failing_after(size_t room) : _room(room) {}

        sgcl::expected<size_t, sgcl::io::error> write(const sgcl::slice<const std::byte>& data) {
            ++calls;
            if (failures || taken + data.size() > _room) {
                ++failures;
                return sgcl::unexpected<sgcl::io::error>(sgcl::io::error(sgcl::error_code(failures == 1 ? EIO : ENOSPC, std::system_category()), "write", "disk"));
            }
            taken += data.size();
            return data.size();
        }

        sgcl::async::task<sgcl::expected<size_t, sgcl::io::error>> async_write(sgcl::slice<const std::byte> data) {
            co_return write(data);
        }

        size_t taken = 0;
        int calls = 0;
        int failures = 0;

    private:
        size_t _room;
    };

    inline bool is_eio(const sgcl::io::error& e) {
        return e.code() == sgcl::error_code(EIO, std::system_category());
    }

    // Texts, random bytes, runs, and the lengths around the window and the match
    inline std::vector<std::pair<std::string, std::string>> corpus() {
        std::vector<std::pair<std::string, std::string>> c;
        for (auto f : {"compress/e.txt", "compress/gettysburg.txt", "compress/pi.txt", "flate/huffman-text.in", "flate/huffman-rand-1k.in"}) {
            c.push_back({f, read_oracle(f)});
        }
        std::mt19937 rng(7);
        std::string random(100000, 0);
        for (auto& ch : random) {
            ch = char(rng());
        }
        c.push_back({"random", random});
        c.push_back({"run", std::string(100000, 'a')});
        c.push_back({"empty", ""});
        c.push_back({"one", "x"});
        for (size_t n : {257, 258, 259, 32767, 32768, 32769, 65535, 65536, 65537}) {
            std::string t(n, 0);
            for (auto& ch : t) {
                ch = "abcab\n"[rng() % 6];
            }
            c.push_back({"len" + std::to_string(n), t});
        }
        return c;
    }

    // zlib's inflate: windowBits -15 raw, 15 zlib, 31 gzip (every member)
    inline bool z_inflate(const std::string& in, int window_bits, std::string& out) {
        z_stream d{};
        inflateInit2(&d, window_bits);
        std::string buf(1 << 16, 0);
        out.clear();
        d.next_in = (Bytef*)in.data();
        d.avail_in = uInt(in.size());
        int st;
        for (;;) {
            d.next_out = (Bytef*)buf.data();
            d.avail_out = uInt(buf.size());
            st = inflate(&d, Z_NO_FLUSH);
            out.append(buf.data(), buf.size() - d.avail_out);
            if (st == Z_STREAM_END && window_bits == 31 && d.avail_in) {
                inflateReset(&d);
                continue;
            }
            if (st != Z_OK) {
                break;
            }
        }
        bool ok = st == Z_STREAM_END && d.avail_in == 0;
        inflateEnd(&d);
        return ok;
    }

    inline std::string z_deflate(const std::string& in, int level, int window_bits, int strategy = Z_DEFAULT_STRATEGY) {
        z_stream z{};
        deflateInit2(&z, level, Z_DEFLATED, window_bits, 8, strategy);
        std::string out(deflateBound(&z, uLong(in.size())) + 64, 0);
        z.next_in = (Bytef*)in.data();
        z.avail_in = uInt(in.size());
        z.next_out = (Bytef*)out.data();
        z.avail_out = uInt(out.size());
        deflate(&z, Z_FINISH);
        out.resize(z.total_out);
        deflateEnd(&z);
        return out;
    }

    // A reader over a string handing out at most `step` bytes a read
    struct dribble {
        std::string data;
        size_t step;
        size_t at = 0;

        sgcl::expected<size_t, sgcl::io::error> read(sgcl::slice<std::byte> b) {
            size_t n = std::min({step, b.size(), data.size() - at});
            std::memcpy(b.data(), data.data() + at, n);
            at += n;
            return n;
        }
    };

    // A directory of its own for files an external tool (bsdtar) reads or
    // makes: under the system's temporary directory with a name no other
    // run takes, removed with everything in it when the test ends — never
    // in the working directory, which may be the repository's root
    class scratch_dir {
    public:
        explicit scratch_dir(const std::string& name) {
            static std::atomic<unsigned> counter{0};
            _path = std::filesystem::temp_directory_path() /
                    (name + "-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
            std::filesystem::remove_all(_path);
            std::filesystem::create_directories(_path);
        }

        ~scratch_dir() {
            std::error_code ec;
            std::filesystem::remove_all(_path, ec);
        }

        scratch_dir(const scratch_dir&) = delete;
        scratch_dir& operator=(const scratch_dir&) = delete;

        const std::filesystem::path& path() const noexcept {
            return _path;
        }

    private:
        std::filesystem::path _path;
    };

    // An external tool, run in a directory: 7zz (7-Zip) and bsdtar
    // (libarchive), the oracles of the 7z tests; its exit status, its
    // output silenced
    inline bool tool_available(const std::string& probe) {
        return std::system((probe + " > /dev/null 2>&1").c_str()) == 0;
    }

    inline int run_in(const std::filesystem::path& dir, const std::string& command) {
        std::string c = "cd '" + dir.string() + "' && " + command + " > /dev/null 2>&1";
        return std::system(c.c_str());
    }

    // 7-Zip's own program: `7zz` on the path, or Homebrew's
    inline const std::string& seven_zip() {
        static const std::string path = tool_available("7zz i") ? "7zz" : tool_available("/opt/homebrew/bin/7zz i") ? "/opt/homebrew/bin/7zz" : "";
        return path;
    }

    inline bool have_bsdtar() {
        static const bool yes = tool_available("bsdtar --version");
        return yes;
    }

    inline std::string slurp(const std::filesystem::path& f) {
        std::ifstream is(f, std::ios::binary);
        std::stringstream ss;
        ss << is.rdbuf();
        return ss.str();
    }
}
