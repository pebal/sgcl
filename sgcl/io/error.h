//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/string.h"

#include <cerrno>
#include <string>
#include <system_error>

namespace sgcl::io {
    // The failures of io that no errno names: the end of a stream where
    // more was required (read_full), a stream closed by the program
    // (a read after close()), an invalid path or pattern, a line past
    // the bound a buffered reader was given. They form the io category
    // beside the system one; every error of the module is a
    // std::error_code of either category.
    enum class errc {
        unexpected_eof = 1,
        closed,
        invalid_path,
        invalid_pattern,
        line_too_long
    };

    namespace detail {
        class IoCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "io";
            }

            std::string message(int c) const override {
                switch (static_cast<errc>(c)) {
                    case errc::unexpected_eof: return "unexpected end of stream";
                    case errc::closed: return "stream closed";
                    case errc::invalid_path: return "invalid path";
                    case errc::invalid_pattern: return "invalid pattern";
                    case errc::line_too_long: return "line too long";
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
    // whatever its category (is_not_found: ENOENT; is_exists: EEXIST;
    // is_permission: EACCES or EPERM; is_closed: errc::closed or EBADF;
    // is_eof: errc::unexpected_eof; is_interrupted: EINTR; is_timeout:
    // ETIMEDOUT or EAGAIN).
    class error {
    public:
        error() = default;

        error(error_code code, const string& op, const string& path = {})
        : _code(code), _op(op), _path(path) {
        }

        error(errc e, const string& op, const string& path = {})
        : error(make_error_code(e), op, path) {
        }

        error_code code() const noexcept {
            return _code;
        }

        const string& op() const noexcept {
            return _op;
        }

        const string& path() const noexcept {
            return _path;
        }

        string message() const {
            std::string m(_op.data(), _op.size());
            if (!_path.empty()) {
                m += ' ';
                m.append(_path.data(), _path.size());
            }
            if (!m.empty()) {
                m += ": ";
            }
            m += _code.message();
            return string(m);
        }

        bool is_not_found() const noexcept {
            return _is(std::errc::no_such_file_or_directory);
        }

        bool is_exists() const noexcept {
            return _is(std::errc::file_exists);
        }

        bool is_permission() const noexcept {
            return _is(std::errc::permission_denied) || _is(std::errc::operation_not_permitted);
        }

        bool is_closed() const noexcept {
            return _code == errc::closed || _is(std::errc::bad_file_descriptor);
        }

        bool is_eof() const noexcept {
            return _code == errc::unexpected_eof;
        }

        bool is_interrupted() const noexcept {
            return _is(std::errc::interrupted);
        }

        bool is_timeout() const noexcept {
            return _is(std::errc::timed_out) || _is(std::errc::resource_unavailable_try_again) || _is(std::errc::operation_would_block);
        }

        friend bool operator==(const error& a, const error& b) noexcept {
            return a._code == b._code;
        }

    private:
        bool _is(std::errc e) const noexcept {
            return _code == std::make_error_condition(e);
        }

        error_code _code;
        string _op;
        string _path;
    };

    // The result of every operation of io: the value or the error.
    // result<void> for an operation that returns nothing.
    template<class T = void>
    using result = expected<T, error>;

    // An error from errno after a failed call: error(errno, op, path)
    inline error last_error(const string& op, const string& path = {}) noexcept {
        return error(error_code(errno, std::system_category()), op, path);
    }

    namespace detail {
        // The error of a failed result, to hand on: `return fail(r);`
        template<class T>
        unexpected<error> fail(const result<T>& r) {
            return unexpected<error>(r.error());
        }

        inline unexpected<error> fail(error e) {
            return unexpected<error>(std::move(e));
        }
    }
}
