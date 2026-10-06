//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/handle_word.h"
#include "error.h"
#include "../core/aliases.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"

#include <atomic>
#include <mutex>
#include <string>
#include <type_traits>

#if !defined(_WIN32)
#include <dlfcn.h>
#else
#include "detail/win_library.h"
#include <string>
#endif

namespace sgcl::io {
    // Dynamic libraries, loaded at run time (dlopen; LoadLibraryW on
    // Windows, written in detail/win_library.h): a plug-in, an optional
    // codec, a system library the program may do without. Go has the plugin
    // package (Go's own plug-ins only, never unloaded); this is the C ABI's.

    // How a library is loaded beyond the one-line form
    struct library_options {
        bool global = false;   // its symbols for the libraries loaded after it (RTLD_GLOBAL); default: its own (RTLD_LOCAL)
        bool lazy = false;     // a function bound at its first call (RTLD_LAZY); default: every one at the load (RTLD_NOW)
    };

    class library;

    namespace detail {
        // The system's handle of a loaded library, the object the handles
        // share. Its destructor does not unload: a function pointer taken
        // from the library is a plain pointer the handle knows nothing of,
        // and a sweep that unloaded the library under a call through one
        // would leave it jumping into unmapped code. close() unloads,
        // explicitly; a library never closed stays loaded to the end of the
        // process, as every library of Go's plugin package does
        class LibraryState final {
        public:
            SGCL_INLINE_HOT LibraryState(void* handle, const string& path) noexcept
            : _handle(handle), _path(path) {
            }

            expected<void*, error> address(const string& name) const noexcept {
                std::lock_guard g(_m);
                if (!_handle) {
                    return detail::fail(error(errc::closed, "symbol", _path));
                }
#if !defined(_WIN32)
                (void)::dlerror();   // the error of a call before this one is not this one's
                void* p = ::dlsym(_handle, name.c_str());
                if (!p) {
                    const char* why = ::dlerror();
                    if (why) {
                        return detail::fail(error(errc::library, "symbol", string(why)));
                    }
                }
                return p;   // a symbol whose value is null is no error
#else
                void* p = win::library_symbol(static_cast<::HINSTANCE__*>(_handle), name.c_str());
                if (!p) {
                    return detail::fail(error(error_code(int(win::GetLastError()), std::system_category()), "symbol", name));
                }
                return p;
#endif
            }

            expected<void, error> close() noexcept {
                std::lock_guard g(_m);
                if (!_handle) {
                    return {};
                }
                void* h = _handle;
                _handle = nullptr;
#if !defined(_WIN32)
                if (::dlclose(h) != 0) {
                    const char* why = ::dlerror();
                    return detail::fail(error(errc::library, "close", string(why ? why : "dlclose")));
                }
#else
                if (win::Dword e = win::free_library(static_cast<::HINSTANCE__*>(h))) {
                    return detail::fail(error(error_code(int(e), std::system_category()), "close", _path));
                }
#endif
                return {};
            }

            SGCL_INLINE_HOT bool is_closed() const noexcept {
                std::lock_guard g(_m);
                return !_handle;
            }

            SGCL_INLINE_HOT const string& path() const noexcept {
                return _path;
            }

        private:
            mutable std::mutex _m;   // a close beside a lookup of another thread
            void* _handle;
            string _path;
        };

        struct LibraryAccess;
    }

    // A loaded library as a handle: one tracked word to the state above,
    // copied and passed by value, the copies one library. Made by
    // open_library; a default-constructed one holds none (`!lib`), and an
    // operation on it is a contract violation.
    class library final {
    public:
        library() noexcept = default;

        // A function of the library, typed: lib.symbol<int(const char*)>("puts").
        // The type is the caller's word: nothing checks it against the
        // library's. The dynamic loader's text for a name it does not know
        // (errc::library); errc::closed after close()
        template<class F>
            requires std::is_function_v<F>
        SGCL_INLINE_HOT expected<F*, error> symbol(const string& name) const noexcept {
            auto p = _get().address(name);
            if (!p) {
                return detail::fail(p);
            }
            return reinterpret_cast<F*>(*p);
        }

        // The address of a symbol: a variable's, or a function's untyped
        SGCL_INLINE_HOT expected<void*, error> address(const string& name) const noexcept {
            return _get().address(name);
        }

        // The library unloaded (dlclose): once; a second does nothing. A
        // pointer taken from it is not to be used after it
        SGCL_INLINE_HOT expected<void, error> close() const noexcept {
            return _get().close();
        }

        SGCL_INLINE_HOT bool is_closed() const noexcept {
            return _get().is_closed();
        }

        // The path or the name it was opened with
        SGCL_INLINE_HOT const string& path() const noexcept {
            return _get().path();
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_state;
        }

        SGCL_INLINE_HOT friend bool operator==(const library& a, const library& b) noexcept {
            return a._state == b._state;
        }

    private:
        friend struct detail::LibraryAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT explicit library(tracked_ptr<detail::LibraryState> state) noexcept
        : _state(std::move(state)) {
        }

        SGCL_INLINE_HOT detail::LibraryState& _get() const noexcept {
            assert(_state && "an empty io::library");
            return *_state;
        }

        SGCL_INLINE_HOT library(sgcl::detail::FromWord, const tracked_ptr<detail::LibraryState>& w) noexcept
        : _state(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::LibraryState>& _handle_word() noexcept {
            return _state;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::LibraryState>& _handle_word() const noexcept {
            return _state;
        }

        tracked_ptr<detail::LibraryState> _state;
    };

    namespace detail {
        struct LibraryAccess {
            SGCL_INLINE_HOT static library make(void* handle, const string& path) noexcept {
                return library(make_tracked<LibraryState>(handle, path));
            }
        };
    }

    // A library loaded (dlopen): a path, or a name the dynamic loader looks
    // for in its paths (library_file_name makes the platform's file name of
    // one); its functions bound at once and its symbols its own unless the
    // options say otherwise. The loader's text (errc::library, as the
    // error's path) when it cannot be loaded
    inline expected<library, error> open_library(const string& path, const library_options& options = {}) noexcept {
#if !defined(_WIN32)
        (void)::dlerror();
        void* h = ::dlopen(path.c_str(), (options.lazy ? RTLD_LAZY : RTLD_NOW) | (options.global ? RTLD_GLOBAL : RTLD_LOCAL));
        if (!h) {
            const char* why = ::dlerror();
            return detail::fail(error(errc::library, "open_library", string(why ? why : "dlopen")));
        }
        return detail::LibraryAccess::make(h, path);
#else
        (void)options;   // Windows binds every import at the load and has no RTLD_GLOBAL
        std::wstring wide = win::wide<std::wstring>(path.c_str(), int(path.size()));
        ::HINSTANCE__* h = win::load_library(wide.c_str());
        if (!h) {
            return detail::fail(error(error_code(int(win::GetLastError()), std::system_category()), "open_library", path));
        }
        return detail::LibraryAccess::make(h, path);
#endif
    }

    // The platform's file name of a library: "libz.dylib" for "z" on macOS,
    // "libz.so" on Linux, "z.dll" on Windows
    SGCL_INLINE_HOT string library_file_name(const string& name) noexcept {
#if defined(_WIN32)
        return name + ".dll";
#elif defined(__APPLE__)
        return string("lib") + name + ".dylib";
#else
        return string("lib") + name + ".so";
#endif
    }
}
