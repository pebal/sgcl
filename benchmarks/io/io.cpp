//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The io module: what a child process costs. One case per run; prints one
// line, ns per operation, for compare.sh (CASES=io).
//
//   io run sgcl [n]         command("true").run(): posix_spawn and a wait, from a thread
//   io output sgcl [n]      command("echo", "hello").output(): a pipe, a copying task, the text
//   io asyncrun sgcl [n]    run() from a task: the exit waited for on the reactor
//   io parallel sgcl [n]    n run() of "true" at once, 32 in flight
//   io read sgcl [n]        read(4 KB) of a file of 1 MB, from the start again at its end
//   io write sgcl [n]       write(4 KB) to a file, from the start again every 1 MB
//   io copy sgcl [n]        io::copy of a file of 1 MB into another
//   io lines sgcl [n]       the lines of a file (100 k of ~40 bytes) through a buffered_reader, per line
//   io byte sgcl [n]        read_byte() of a buffered_reader over a file of 1 MB
//   io buf sgcl [n]         write(1 byte) to an io::buffer, cleared every 4 KB
//   io bufw sgcl [n]        write(16 bytes) to a buffered_writer over io::discard
// The file cases are written so that one source builds before and after
// the io handles (an A/B of the same code): the free functions of io and
// a helper that reaches the file through either shape of io::open's value.
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"

using namespace sgcl::async;

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

namespace {
    sgcl::async::task<long> async_runs(long n) {
        long ok = 0;
        for (long i = 0; i < n; ++i) {
            sgcl::io::command c("true");
            ok += (bool)co_await c.async_run();
        }
        co_return ok;
    }

    sgcl::async::task<long> parallel_runs(long n, long width) {
        long ok = 0;
        for (long done = 0; done < n; done += width) {
            sgcl::vector<sgcl::async::task<sgcl::expected<void, sgcl::io::error>>> batch;
            sgcl::vector<sgcl::io::command> commands;
            for (long i = 0; i < width && done + i < n; ++i) {
                commands.push_back(sgcl::io::command("true"));
            }
            for (auto& c : commands) {
                batch.push_back(sgcl::async::spawn(c.async_run()));
            }
            for (auto& t : batch) {
                ok += (bool)co_await t;
            }
        }
        co_return ok;
    }

    // The file behind io::open's value: a tracked_ptr to it before the
    // handles, the handle itself after
    template<class F>
    decltype(auto) file_of(F& f) {
        if constexpr (requires { f->fd(); }) {
            return *f;
        } else {
            return (f);
        }
    }

    std::string temp_path(const char* what) {
        const char* t = std::getenv("TMPDIR");
        std::string dir = t && *t ? t : "/tmp";
        if (dir.back() != '/') {
            dir += '/';
        }
        return dir + "sgcl_bench_io_" + what + "_" + std::to_string(::getpid());
    }

    void make_file(const std::string& path, size_t size) {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        std::string block(4096, 'x');
        for (size_t done = 0; done < size; done += block.size()) {
            std::fwrite(block.data(), 1, block.size(), f);
        }
        std::fclose(f);
    }

    void make_lines(const std::string& path, long lines) {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        for (long i = 0; i < lines; ++i) {
            std::fprintf(f, "line %08ld of the benchmark's text file\n", i);
        }
        std::fclose(f);
    }

    void report(const char* what, double wall, long ops) {
        std::printf("io %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }
}

int main(int argc, char** argv) {
    if (argc < 3 || std::string(argv[2]) != "sgcl") {
        std::fprintf(stderr, "usage: io <run|output|asyncrun|parallel|read|write|copy|lines|byte|buf|bufw> sgcl [n]\n");
        return 2;
    }
    std::string what = argv[1];
    long n = argc > 3 ? std::atol(argv[3]) : 0;
    long ok = 0;
    if (what == "run") {
        n = n ? n : 1000;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            sgcl::io::command c("true");
            ok += (bool)c.run();
        }
        report("run", bench::seconds_since(t0), n);
    } else if (what == "output") {
        n = n ? n : 1000;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            sgcl::io::command c("echo", "hello");
            auto out = c.output();
            ok += out && out->size() == 6;
        }
        report("output", bench::seconds_since(t0), n);
    } else if (what == "asyncrun") {
        n = n ? n : 1000;
        auto t0 = bench::Clock::now();
        ok = sgcl::async::spawn(async_runs(n)).wait();
        report("asyncrun", bench::seconds_since(t0), n);
    } else if (what == "parallel") {
        n = n ? n : 2000;
        auto t0 = bench::Clock::now();
        ok = sgcl::async::spawn(parallel_runs(n, 32)).wait();
        report("parallel", bench::seconds_since(t0), n);
    } else if (what == "read") {
        n = n ? n : 2500000;
        auto path = temp_path("read");
        make_file(path, 1 << 20);
        auto f = sgcl::io::open(sgcl::string(path.c_str()));
        auto& file = file_of(*f);
        sgcl::vector<std::byte> block(4096);
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            auto r = file.read(block.as_slice());
            if (r && *r == 0) {
                (void)file.seek(0);
                r = file.read(block.as_slice());
            }
            ok += r && *r == 4096;
        }
        report("read", bench::seconds_since(t0), n);
        (void)file.close();
        std::remove(path.c_str());
    } else if (what == "write") {
        n = n ? n : 2000000;
        auto path = temp_path("write");
        auto f = sgcl::io::create(sgcl::string(path.c_str()));
        auto& file = file_of(*f);
        sgcl::vector<std::byte> block(4096);
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            if (i % 256 == 0) {
                (void)file.seek(0);
            }
            auto r = file.write(block.as_slice());
            ok += r && *r == 4096;
        }
        report("write", bench::seconds_since(t0), n);
        (void)file.close();
        std::remove(path.c_str());
    } else if (what == "copy") {
        n = n ? n : 15000;
        auto from = temp_path("copy_from"), to = temp_path("copy_to");
        make_file(from, 1 << 20);
        auto f = sgcl::io::open(sgcl::string(from.c_str()));
        auto g = sgcl::io::create(sgcl::string(to.c_str()));
        auto& source = file_of(*f);
        auto& target = file_of(*g);
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            (void)source.seek(0);
            (void)target.seek(0);
            auto r = sgcl::io::copy(*g, *f);
            ok += r && *r == (1 << 20);
        }
        report("copy", bench::seconds_since(t0), n);
        (void)source.close();
        (void)target.close();
        std::remove(from.c_str());
        std::remove(to.c_str());
    } else if (what == "lines") {
        n = n ? n : 40000000;
        long per_file = 100000;
        auto path = temp_path("lines");
        make_lines(path, per_file);
        auto f = sgcl::io::open(sgcl::string(path.c_str()));
        auto& file = file_of(*f);
        long seen = 0;
        auto t0 = bench::Clock::now();
        while (seen < n) {
            (void)file.seek(0);
            sgcl::io::buffered_reader in{sgcl::io::reader(*f)};
            for (auto line : in.lines()) {
                seen += line.size() > 0;
            }
        }
        report("lines", bench::seconds_since(t0), seen);
        ok = n;
        (void)file.close();
        std::remove(path.c_str());
    } else if (what == "byte") {
        n = n ? n : 600000000;
        auto path = temp_path("byte");
        make_file(path, 1 << 20);
        auto f = sgcl::io::open(sgcl::string(path.c_str()));
        auto& file = file_of(*f);
        long done = 0;
        auto t0 = bench::Clock::now();
        while (done < n) {
            (void)file.seek(0);
            sgcl::io::buffered_reader in{sgcl::io::reader(*f)};
            for (;;) {
                auto b = in.read_byte();
                if (!b || !*b) {
                    break;
                }
                ++done;
            }
        }
        report("byte", bench::seconds_since(t0), done);
        ok = n;
        (void)file.close();
        std::remove(path.c_str());
    } else if (what == "buf") {
        n = n ? n : 300000000;
        sgcl::io::buffer out;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            if ((i & 4095) == 0) {
                out.clear();
            }
            ok += (bool)out.write(std::byte(i));
        }
        report("buf", bench::seconds_since(t0), n);
    } else if (what == "bufw") {
        n = n ? n : 300000000;
        sgcl::io::buffered_writer out{sgcl::io::writer(sgcl::io::discard)};
        std::byte chunk[16] = {};
        sgcl::slice<const std::byte> data(chunk, 16);
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            ok += (bool)out.write(data);
        }
        (void)out.flush();
        report("bufw", bench::seconds_since(t0), n);
    } else {
        std::fprintf(stderr, "unknown case %s\n", what.c_str());
        return 2;
    }
    return ok == n ? 0 : 1;
}
