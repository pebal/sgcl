//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "file.h"
#include "../core/root_ptr.h"
#include "../core/vector.h"
#include "../core/aliases.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <pwd.h>
#include <string>
#include <string_view>
#include <unistd.h>
#if defined(__APPLE__)
#include <crt_externs.h>
#include <mach-o/dyld.h>
#endif

namespace sgcl::io {
    // The process and its environment (os): arguments, variables,
    // directories the platform names, the standard streams.

    // The command line, argv[0] first: from the platform (the loader's
    // copy on macOS, /proc on Linux), no main needed
    inline vector<string> args() {
        vector<string> out;
#if defined(__APPLE__)
        int argc = *_NSGetArgc();
        char** argv = *_NSGetArgv();
        for (int i = 0; i < argc; ++i) {
            out.push_back(string(argv[i]));
        }
#else
        if (auto r = detail::_block_read_file("/proc/self/cmdline")) {
            auto text = detail::chars_of(as_bytes(r->as_slice()));
            size_t pos = 0;
            while (pos < text.size()) {
                size_t end = text.find('\0', pos);
                if (end == std::string_view::npos) {
                    end = text.size();
                }
                out.push_back(string(text.substr(pos, end - pos)));
                pos = end + 1;
            }
        }
#endif
        return out;
    }

    // A variable: its value, nullopt when unset (an empty value is a
    // value); set and unset
    inline optional<string> getenv(const string& name) {
        const char* v = ::getenv(name.c_str());
        if (!v) {
            return nullopt;
        }
        return string(v);
    }

    inline expected<void, error> setenv(const string& name, const string& value) {
        if (::setenv(name.c_str(), value.c_str(), 1) != 0) {
            return detail::fail(last_error("setenv", name));
        }
        return {};
    }

    inline expected<void, error> unsetenv(const string& name) {
        if (::unsetenv(name.c_str()) != 0) {
            return detail::fail(last_error("unsetenv", name));
        }
        return {};
    }

    inline vector<pair<string, string>> environ() {
#if defined(__APPLE__)
        char** env = *_NSGetEnviron();
#else
        char** env = ::environ;
#endif
        vector<pair<string, string>> out;
        for (; env && *env; ++env) {
            std::string_view e(*env);
            auto eq = e.find('=');
            if (eq == std::string_view::npos) {
                out.push_back({string(e), string()});
            } else {
                out.push_back({string(e.substr(0, eq)), string(e.substr(eq + 1))});
            }
        }
        return out;
    }

    // "$NAME" and "${NAME}" in s replaced by the variable, unset ones by
    // ""; a name is letters, digits and '_'
    inline string expand_env(const string& text) {
        std::string_view s(text);
        std::string out;
        auto is_name = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
        for (size_t i = 0; i < s.size();) {
            if (s[i] != '$' || i + 1 == s.size()) {
                out += s[i++];
                continue;
            }
            std::string_view name;
            size_t next;
            if (s[i + 1] == '{') {
                auto close = s.find('}', i + 2);
                if (close == std::string_view::npos) {
                    out += s[i++];
                    continue;
                }
                name = s.substr(i + 2, close - i - 2);
                next = close + 1;
            } else {
                size_t j = i + 1;
                while (j < s.size() && is_name(s[j])) {
                    ++j;
                }
                if (j == i + 1) {
                    out += s[i++];
                    continue;
                }
                name = s.substr(i + 1, j - i - 1);
                next = j;
            }
            if (auto v = getenv(string(name))) {
                out.append(v->data(), v->size());
            }
            i = next;
        }
        return string(out);
    }

    inline expected<string, error> working_dir() {
        char buf[4096];
        if (!::getcwd(buf, sizeof buf)) {
            return detail::fail(last_error("getcwd"));
        }
        return string(buf);
    }

    inline expected<void, error> chdir(const string& path) {
        if (::chdir(path.c_str()) != 0) {
            return detail::fail(last_error("chdir", path));
        }
        return {};
    }

    // The user's home ($HOME, else the password database); the
    // platform's directories for a program's cache and configuration
    // (~/Library/Caches and ~/Library/Application Support on macOS,
    // $XDG_CACHE_HOME or ~/.cache and $XDG_CONFIG_HOME or ~/.config
    // elsewhere); the temporary directory ($TMPDIR, else /tmp)
    inline expected<string, error> home_dir() {
        if (auto h = getenv("HOME"); h && !h->empty()) {
            return *h;
        }
        if (auto pw = ::getpwuid(::getuid()); pw && pw->pw_dir) {
            return string(pw->pw_dir);
        }
        return detail::fail(error(std::make_error_code(std::errc::no_such_file_or_directory), "home_dir"));
    }

    inline expected<string, error> cache_dir() {
#if defined(__APPLE__)
        auto h = home_dir();
        if (!h) {
            return h;
        }
        return io::path::join(*h, "Library/Caches");
#else
        if (auto x = getenv("XDG_CACHE_HOME"); x && !x->empty()) {
            return *x;
        }
        auto h = home_dir();
        if (!h) {
            return h;
        }
        return io::path::join(*h, ".cache");
#endif
    }

    inline expected<string, error> config_dir() {
#if defined(__APPLE__)
        auto h = home_dir();
        if (!h) {
            return h;
        }
        return io::path::join(*h, "Library/Application Support");
#else
        if (auto x = getenv("XDG_CONFIG_HOME"); x && !x->empty()) {
            return *x;
        }
        auto h = home_dir();
        if (!h) {
            return h;
        }
        return io::path::join(*h, ".config");
#endif
    }

    inline string temp_dir() {
        return detail::temp_root();
    }

    // The running executable's path, symlinks resolved; the host's
    // name; the process id
    inline expected<string, error> executable() {
        char buf[4096];
#if defined(__APPLE__)
        uint32_t size = sizeof buf;
        if (_NSGetExecutablePath(buf, &size) != 0) {
            return detail::fail(error(std::make_error_code(std::errc::filename_too_long), "executable"));
        }
        char real[PATH_MAX];
        if (!::realpath(buf, real)) {
            return detail::fail(last_error("executable", string(buf)));
        }
        return string(real);
#else
        ssize_t n = ::readlink("/proc/self/exe", buf, sizeof buf - 1);
        if (n < 0) {
            return detail::fail(last_error("executable", "/proc/self/exe"));
        }
        return string(std::string_view(buf, static_cast<size_t>(n)));
#endif
    }

    inline expected<string, error> hostname() {
        char buf[256];
        if (::gethostname(buf, sizeof buf) != 0) {
            return detail::fail(last_error("hostname"));
        }
        buf[sizeof buf - 1] = 0;
        return string(buf);
    }

    inline int pid() noexcept {
        return static_cast<int>(::getpid());
    }

    // The standard streams, io::stdin, io::stdout and io::stderr: three
    // objects, constant-initialized, each over a file of descriptor 0, 1
    // or 2 made the first time the stream is used and kept for the life
    // of the process, the descriptor never closed by it; a descriptor
    // that is non-blocking already is served by the reactor, any other
    // (a terminal, a redirected file, a pipe from the shell) by the
    // blocking pool, its flags left as they are.
    //
    // <cstdio> defines stdin, stdout and stderr as macros (on macOS for
    // the variables __stdinp, __stdoutp, __stderrp; on glibc for
    // variables of the same names). The macros go, the framework's
    // streams take the names, and the C streams stay
    // reachable under the same names as references in the global
    // scope, so that code written for <stdio.h> compiles on. Where the
    // variable already carries the name (glibc), the #undef alone does
    // it. A unit with `using namespace sgcl::io` that writes a bare
    // `stderr` for the C stream finds both and must qualify one.
#if defined(stdin) && !defined(__GLIBC__)
}
namespace {
    FILE*& sgcl_c_stdin = stdin;
    FILE*& sgcl_c_stdout = stdout;
    FILE*& sgcl_c_stderr = stderr;
#undef stdin
#undef stdout
#undef stderr
    FILE*& stdin = sgcl_c_stdin;
    FILE*& stdout = sgcl_c_stdout;
    FILE*& stderr = sgcl_c_stderr;
}
namespace sgcl::io {
#else
#undef stdin
#undef stdout
#undef stderr
#endif

    class standard_stream final
    : public mixin::reader<standard_stream>
    , public mixin::writer<standard_stream> {
    public:
        using mixin::writer<standard_stream>::write;
        using mixin::writer<standard_stream>::async_write;

        constexpr standard_stream(int fd, const char* name) noexcept
        : _fd(fd), _name(name) {
        }

        standard_stream(const standard_stream&) = delete;
        standard_stream& operator=(const standard_stream&) = delete;

        expected<size_t, error> read(const slice<byte>& buffer) {
            return _get().read(buffer);
        }

        async::task<expected<size_t, error>> async_read(const slice<byte>& buffer) {
            return _get().async_read(buffer);
        }

        expected<size_t, error> write(const slice<const byte>& data) {
            return _get().write(data);
        }

        async::task<expected<size_t, error>> async_write(const slice<const byte>& data) {
            return _get().async_write(data);
        }

        // The file over the descriptor, for what takes a file (a child's
        // standard stream shared with the program's: cmd.out = io::stdout.file())
        tracked_ptr<io::file> file() const {
            return _held().ptr();
        }

        int fd() const noexcept {
            return _fd;
        }

        bool is_terminal() const noexcept {
            return ::isatty(_fd) == 1;
        }

    private:
        io::file& _get() const {
            return *_held();
        }

        // Made once, on the first use, by whichever thread comes first;
        // never destroyed: a root whose cell outlives the static
        // destructors that may still write to the stream
        root_ptr<io::file>& _held() const {
            root_ptr<io::file>* p = _file.load(std::memory_order_acquire);
            if (!p) {
                auto made = new root_ptr<io::file>(detail::std_stream(_fd, _name));
                if (_file.compare_exchange_strong(p, made, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    p = made;
                } else {
                    delete made;
                }
            }
            return *p;
        }

        int _fd;
        const char* _name;
        mutable std::atomic<root_ptr<io::file>*> _file = {nullptr};
    };

    inline standard_stream stdin(0, "stdin");
    inline standard_stream stdout(1, "stdout");
    inline standard_stream stderr(2, "stderr");

    // Whether the descriptor is a terminal
    inline bool is_terminal(int fd) noexcept {
        return ::isatty(fd) == 1;
    }

    // Ends the process with the code now: the C streams flushed, no
    // destructor run
    [[noreturn]] inline void exit(int code) {
        std::fflush(nullptr);
        ::_exit(code);
    }
}
