//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/handle_word.h"
#include "detail/mapped_region.h"

#include <cassert>
#include <cstdint>

#if !defined(_WIN32)
#include <fcntl.h>
#endif

namespace sgcl::io {
    class shared_memory;

    namespace detail {
        struct SharedMemoryAccess;
    }

    // A named region of memory shared between processes (DESIGN 289): a
    // handle of one word, a tracked_ptr to the region inside
    // (detail::MappedRegion, the object io::mapping holds too), whose
    // copies share it. create(name, size) makes the object and maps it,
    // open(name) maps an existing one, remove(name) takes the name away.
    // data() is a slice<byte> (the region is always read and written)
    // holding the region as its owner: a slice kept after the last handle
    // still reads the region, unmapped when nothing holds it any more.
    //
    // The name is a word of the program's: not empty, no '/' or '\\'. On
    // POSIX it is "/name" to shm_open, the object made 0600 (the user's
    // own processes); it lives until remove(name), past the processes,
    // until a reboot. On Windows it is "Local\\name" to
    // CreateFileMappingW (the session's namespace: "Global\\" needs a
    // privilege a program seldom has); the object lives until the last
    // handle to it, a view included, is closed, and remove(name) has
    // nothing to do. A default-constructed one holds no region (`!m`),
    // and an operation on it is a contract violation.
    class shared_memory final {
    public:
        shared_memory() noexcept = default;

        // A new object of `size` bytes (zeros) under the name, mapped for
        // reading and writing: is_exists() when the name is taken, EINVAL
        // for a size of 0 or a bad name
        static expected<shared_memory, error> create(const string& name, size_t size);

        // The object of the name, mapped for reading and writing:
        // is_not_found() when there is none. Its size is the object's as
        // the system keeps it, which macOS rounds up to a page
        static expected<shared_memory, error> open(const string& name);

        // The name taken away (shm_unlink): processes that mapped the
        // object keep it; a create of the name makes a new one.
        // is_not_found() when there is none. Nothing on Windows
        static expected<void, error> remove(const string& name);

        // The region's bytes; empty once closed
        slice<byte> data() const noexcept {
            auto& r = _get();
            if (r.size() == 0) {
                return {};
            }
            return slice<byte>(_region, r.begin(), r.begin() + r.size(), sgcl::detail::OutsideOwner{});
        }

        // The length, 0 once closed
        size_t size() const noexcept {
            return _get().size();
        }

        // The region given back now, as mapping::close(): the descriptor
        // (the handles on Windows) closed, the range left as zeros for a
        // slice taken before; data() is empty after it. The object and its
        // name are untouched (remove(name)). The destructor unmaps a region
        // nobody closed; a second close does nothing
        expected<void, error> close() const {
            return _get().close();
        }

        bool is_closed() const noexcept {
            return _get().closed();
        }

        // Whether this handle holds a region
        explicit operator bool() const noexcept {
            return (bool)_region;
        }

        // The same region (not the same name: two opens are two regions)
        friend bool operator==(const shared_memory& a, const shared_memory& b) noexcept {
            return a._region == b._region;
        }

    private:
        friend struct detail::SharedMemoryAccess;

        explicit shared_memory(tracked_ptr<detail::MappedRegion> region) noexcept
        : _region(std::move(region)) {
        }

        detail::MappedRegion& _get() const noexcept {
            assert(_region && "an empty io::shared_memory");
            return *_region;
        }

        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        shared_memory(sgcl::detail::FromWord, const tracked_ptr<detail::MappedRegion>& w) noexcept
        : _region(w) {
        }

        tracked_ptr<detail::MappedRegion>& _handle_word() noexcept {
            return _region;
        }

        const tracked_ptr<detail::MappedRegion>& _handle_word() const noexcept {
            return _region;
        }

        tracked_ptr<detail::MappedRegion> _region;
    };

    namespace detail {
        struct SharedMemoryAccess {
            static shared_memory make(tracked_ptr<MappedRegion> region) noexcept {
                return shared_memory(std::move(region));
            }
        };

        // A name the object may have: not empty, no separator, no NUL
        inline bool valid_shared_name(const string& name) noexcept {
            if (name.empty()) {
                return false;
            }
            for (size_t i = 0; i < name.size(); ++i) {
                char c = name.data()[i];
                if (c == '/' || c == '\\' || c == '\0') {
                    return false;
                }
            }
            return true;
        }

        inline unexpected<error> bad_shared_name(const char* op, const string& name) {
            return fail(error(errc::invalid_path, op, name));
        }

#if defined(_WIN32)
        inline std::wstring shared_name(const string& name) {
            std::string n = "Local\\";
            n.append(name.data(), name.size());
            return win::wide<std::wstring>(n.data(), int(n.size()));
        }

        // The object's view mapped, the section kept with it
        inline expected<shared_memory, error> map_section(win::Handle section, size_t size, const string& name) {
            void* base = win::MapViewOfFile(section, win::FileMapRead | win::FileMapWrite, 0, 0, size);
            if (!base) {
                auto e = MappedRegion::windows_error("map", name);
                win::CloseHandle(section);
                return fail(e);
            }
            if (size == 0) {
                win::MemoryInfo info{};
                if (win::VirtualQuery(base, reinterpret_cast<::_MEMORY_BASIC_INFORMATION*>(&info), sizeof(info)) == 0) {
                    auto e = MappedRegion::windows_error("map", name);
                    win::UnmapViewOfFile(base);
                    win::CloseHandle(section);
                    return fail(e);
                }
                size = info.region_size;
            }
            return SharedMemoryAccess::make(make_tracked<MappedRegion>(base, size, static_cast<byte*>(base), size, win::InvalidHandle, section, true, true, name));
        }
#else
        inline std::string shared_name(const string& name) {
            std::string n = "/";
            n.append(name.data(), name.size());
            return n;
        }

        // The object's descriptor mapped (the region's from now on);
        // `created`: removed again on failure
        inline expected<shared_memory, error> map_shared(int fd, size_t size, const string& name, bool created) {
            auto give_back = [&] {
                ::close(fd);
                if (created) {
                    ::shm_unlink(shared_name(name).c_str());
                }
            };
            if (size == 0) {
                return SharedMemoryAccess::make(make_tracked<MappedRegion>(nullptr, 0, nullptr, 0, fd, true, true, name));
            }
            void* base = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (base == MAP_FAILED) {
                auto e = last_error("mmap", name);
                give_back();
                return fail(e);
            }
            return SharedMemoryAccess::make(make_tracked<MappedRegion>(base, size, static_cast<byte*>(base), size, fd, true, true, name));
        }
#endif
    }

    inline expected<shared_memory, error> shared_memory::create(const string& name, size_t size) {
        if (!detail::valid_shared_name(name)) {
            return detail::bad_shared_name("create", name);
        }
        if (size == 0) {
            return detail::fail(error(error_code(EINVAL, std::generic_category()), "create", name));   // mmap maps nothing of 0 bytes
        }
#if defined(_WIN32)
        auto n = detail::shared_name(name);
        uint64_t s = size;
        win::Handle section = win::CreateFileMappingW(win::InvalidHandle, nullptr, win::PageReadwrite, win::Dword(s >> 32), win::Dword(s), n.c_str());
        if (!section) {
            return detail::fail(detail::MappedRegion::windows_error("create", name));
        }
        if (win::GetLastError() == win::ErrorAlreadyExists) {
            win::CloseHandle(section);
            return detail::fail(error(error_code(EEXIST, std::generic_category()), "create", name));
        }
        return detail::map_section(section, size, name);
#else
        auto n = detail::shared_name(name);
        int fd = ::shm_open(n.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);   // FD_CLOEXEC set by shm_open itself (POSIX); macOS refuses the flag
        if (fd < 0) {
            return detail::fail(last_error("create", name));
        }
        if (::ftruncate(fd, off_t(size)) != 0) {
            auto e = last_error("create", name);
            ::close(fd);
            ::shm_unlink(n.c_str());
            return detail::fail(e);
        }
        return detail::map_shared(fd, size, name, true);
#endif
    }

    inline expected<shared_memory, error> shared_memory::open(const string& name) {
        if (!detail::valid_shared_name(name)) {
            return detail::bad_shared_name("open", name);
        }
#if defined(_WIN32)
        auto n = detail::shared_name(name);
        win::Handle section = win::OpenFileMappingW(win::FileMapRead | win::FileMapWrite, 0, n.c_str());
        if (!section) {
            return detail::fail(detail::MappedRegion::windows_error("open", name));
        }
        return detail::map_section(section, 0, name);
#else
        int fd = ::shm_open(detail::shared_name(name).c_str(), O_RDWR, 0);
        if (fd < 0) {
            return detail::fail(last_error("open", name));
        }
        struct ::stat st;
        if (::fstat(fd, &st) != 0) {
            auto e = last_error("open", name);
            ::close(fd);
            return detail::fail(e);
        }
        return detail::map_shared(fd, size_t(st.st_size), name, false);
#endif
    }

    inline expected<void, error> shared_memory::remove(const string& name) {
        if (!detail::valid_shared_name(name)) {
            return detail::bad_shared_name("remove", name);
        }
#if defined(_WIN32)
        return {};
#else
        if (::shm_unlink(detail::shared_name(name).c_str()) != 0) {
            return detail::fail(last_error("remove", name));
        }
        return {};
#endif
    }
}
