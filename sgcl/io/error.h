//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/string.h"

#include <cerrno>
#include <cstdint>
#include <string>
#include <system_error>

namespace sgcl::io {
    // The failures of io that no errno names: the end of a stream part way
    // through what was required (read_full), a stream closed by the program
    // (a read after close()), an invalid path or pattern, a line past
    // the bound a buffered reader was given. They form the io category
    // beside the system one; every error of the module is a
    // std::error_code of either category.
    enum class errc {
        unexpected_eof = 1,
        closed,
        invalid_path,
        invalid_pattern,
        line_too_long,
        not_found,        // look_path: no executable of the name in PATH
        exit_status,      // a process ended with a failure status: the code in the command's state
        process_done,     // the process was waited for or released already
        wait_delay,       // the wait ended by wait_delay with the child's pipes still open
        unsupported,      // a descriptor the reactor cannot watch: its number past the reactor's table
        insecure_path,    // a name that would leave its directory once joined to it (path::under), as Go's ErrInsecurePath
        invalid_argument, // flags: a command line the flags do not take (the text as Go's flag package writes it, in the path)
        help_requested    // flags: -h or -help asked for the usage (Go's flag.ErrHelp)
    };

    namespace detail {
        class IoCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "io";
            }

            std::string message(int c) const noexcept override {
                switch (static_cast<errc>(c)) {
                    case errc::unexpected_eof: return "unexpected end of stream";
                    case errc::closed: return "stream closed";
                    case errc::invalid_path: return "invalid path";
                    case errc::invalid_pattern: return "invalid pattern";
                    case errc::line_too_long: return "line too long";
                    case errc::not_found: return "executable file not found in PATH";
                    case errc::exit_status: return "the process ended with a failure status";
                    case errc::process_done: return "process already finished";
                    case errc::wait_delay: return "wait delay expired";
                    case errc::unsupported: return "descriptor number past the reactor's table";
                    case errc::insecure_path: return "insecure path";
                    case errc::invalid_argument: return "invalid command line";
                    case errc::help_requested: return "help requested";
                }
                return "unknown io error";
            }
        };
    }

    inline const std::error_category& category() noexcept {
        static const detail::IoCategory instance;
        return instance;
    }

    inline error_code make_error_code(errc e) noexcept {
        return error_code(static_cast<int>(e), category());
    }
}

template<>
struct std::is_error_code_enum<sgcl::io::errc> : std::true_type {};

namespace sgcl::io {
    // What an operation of io reports when it fails: the code (errno or
    // errc, in its category), the operation ("open", "read", "mkdir") and
    // the path or the description of the stream it was on, so that
    // message() reads "open log.txt: no such file or directory", as
    // Go's *PathError. A value: copied, compared by code, held in an
    // expected. The predicates ask the question that the code answers
    // whatever its category (is_not_found: ENOENT or errc::not_found,
    // look_path's; is_exists: EEXIST;
    // is_permission: EACCES or EPERM; is_closed: errc::closed or EBADF;
    // is_eof: errc::unexpected_eof; is_interrupted: EINTR; is_timeout:
    // ETIMEDOUT or EAGAIN). count() is what the operation had done when it
    // failed: the bytes read_full read before the stream ended (Go's n
    // beside io.ErrUnexpectedEOF); 0 for the others.
    //
    // The code is held as its two parts, the count in the four bytes an
    // error_code leaves free between them: four words, as before the count,
    // so that an expected<T, error> on the paths of every read and write
    // grew by nothing (a fifth word added a store to read_byte's return). A
    // count past 32 bits is held as UINT32_MAX.
    class error {
    public:
        error() noexcept = default;

        error(error_code code, const string& op, const string& path = {}, size_t count = 0) noexcept
        : _value(code.value())
        , _count(count < UINT32_MAX ? static_cast<uint32_t>(count) : UINT32_MAX)
        , _category(&code.category())
        , _op(op)
        , _path(path) {
        }

        error(errc e, const string& op, const string& path = {}, size_t count = 0) noexcept
        : error(make_error_code(e), op, path, count) {
        }

        error_code code() const noexcept {
            return error_code(_value, *_category);
        }

        const string& op() const noexcept {
            return _op;
        }

        const string& path() const noexcept {
            return _path;
        }

        size_t count() const noexcept {
            return _count;
        }

        string message() const noexcept {
            std::string m(_op.data(), _op.size());
            if (!_path.empty()) {
                if (!m.empty()) {
                    m += ' ';
                }
                m.append(_path.data(), _path.size());
            }
            if (!m.empty()) {
                m += ": ";
            }
            m += _category->message(_value);
            return string(m);
        }

        bool is_not_found() const noexcept {
            return _is(std::errc::no_such_file_or_directory) || code() == errc::not_found;
        }

        bool is_exists() const noexcept {
            return _is(std::errc::file_exists);
        }

        bool is_permission() const noexcept {
            return _is(std::errc::permission_denied) || _is(std::errc::operation_not_permitted);
        }

        bool is_closed() const noexcept {
            return code() == errc::closed || _is(std::errc::bad_file_descriptor);
        }

        bool is_eof() const noexcept {
            return code() == errc::unexpected_eof;
        }

        bool is_interrupted() const noexcept {
            return _is(std::errc::interrupted);
        }

        bool is_timeout() const noexcept {
            return _is(std::errc::timed_out) || _is(std::errc::resource_unavailable_try_again) || _is(std::errc::operation_would_block);
        }

        bool is_exit_status() const noexcept {
            return code() == errc::exit_status;
        }

        friend bool operator==(const error& a, const error& b) noexcept {
            return a.code() == b.code();
        }

    private:
        bool _is(std::errc e) const noexcept {
            return code() == std::make_error_condition(e);
        }

        int _value = 0;
        uint32_t _count = 0;
        const std::error_category* _category = &std::system_category();
        string _op;
        string _path;
    };

    // An error from errno after a failed call: error(errno, op, path)
    inline error last_error(const string& op, const string& path = {}) noexcept {
        return error(error_code(errno, std::system_category()), op, path);
    }

    namespace detail {
        // The error of a failed result, to hand on: `return fail(r);`
        template<class T>
        unexpected<error> fail(const expected<T, error>& r) noexcept {
            return unexpected<error>(r.error());
        }

        inline unexpected<error> fail(error e) noexcept {
            return unexpected<error>(std::move(e));
        }
    }
}
