//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/make_tracked.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../error.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

#if defined(_WIN32)
#include "win_mapping.h"
#else
#include <cerrno>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace sgcl::io::detail {
    // A region of memory mapped from a file or from a named shared memory
    // object (DESIGN 289): the object under io::mapping and
    // io::shared_memory alike, which are one tracked word to it. Managed
    // (make_tracked), so that a slice of the region holds it as its owner
    // (slice.h: OutsideOwner) and the mapping lives for as long as any
    // slice of it or any handle does. The destructor, on the collector's
    // thread after the sweep that finds the region dead, unmaps it and
    // closes the descriptor (the view and the handles on Windows); a
    // shared memory object's name is not removed then: that is
    // shared_memory::remove, explicitly.
    //
    // The mapping starts at `base` (a page, or on Windows an allocation
    // granule, at or before the range asked for) and spans `mapped` bytes;
    // the range handed out is [data, data + size) within it. A region of
    // size 0 (an empty file, an empty range) has no mapping at all.
    //
    // close() gives the file back at once without leaving a slice taken
    // before it pointing at nothing: on POSIX the range is replaced, in
    // one mmap with MAP_FIXED, by anonymous pages of the same protection
    // (zeros, untouched pages cost nothing), and the descriptor is closed;
    // the address range itself is given back by the destructor. On
    // Windows, which has no such replacement in one call, close() closes
    // the handles and the view stays until the destructor.
    //
    // The memory is outside the managed heap: the collector never looks
    // into it, so only trivial data lies there, never a tracked word.
    class MappedRegion final {
    public:
#if defined(_WIN32)
        using Handle = win::Handle;

        SGCL_INLINE_HOT MappedRegion(void* base, size_t mapped, byte* data, size_t size, Handle file, Handle section, bool writable, bool shared, const string& name) noexcept
        : _base(base), _mapped(mapped), _data(data), _size(size), _file(file), _section(section), _writable(writable), _shared(shared), _name(name) {
        }
#else
        SGCL_INLINE_HOT MappedRegion(void* base, size_t mapped, byte* data, size_t size, int fd, bool writable, bool shared, const string& name) noexcept
        : _base(base), _mapped(mapped), _data(data), _size(size), _fd(fd), _writable(writable), _shared(shared), _name(name) {
        }
#endif

        MappedRegion(const MappedRegion&) = delete;
        MappedRegion& operator=(const MappedRegion&) = delete;

        SGCL_INLINE_HOT ~MappedRegion() {
#if defined(_WIN32)
            if (_base) {
                win::UnmapViewOfFile(_base);
            }
            if (!_closed.load(std::memory_order_acquire)) {
                _close_handles();
            }
#else
            if (_base) {
                ::munmap(_base, _mapped);
            }
            if (!_closed.load(std::memory_order_acquire) && _fd >= 0) {
                ::close(_fd);
            }
#endif
        }

        // The range handed out, [begin, begin + size); nothing once closed
        SGCL_INLINE_HOT byte* begin() const noexcept {
            return _closed.load(std::memory_order_acquire) ? nullptr : _data;
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            return _closed.load(std::memory_order_acquire) ? 0 : _size;
        }

        SGCL_INLINE_HOT bool writable() const noexcept {
            return _writable;
        }

        SGCL_INLINE_HOT bool closed() const noexcept {
            return _closed.load(std::memory_order_acquire);
        }

        // The writes of a writable shared region given to the file and
        // waited for (msync MS_SYNC; FlushViewOfFile and FlushFileBuffers);
        // nothing to do for a region read only, private, or empty
        expected<void, error> flush() noexcept {
            std::lock_guard lock(_mutex);
            if (_closed.load(std::memory_order_relaxed)) {
                return fail(error(errc::closed, "flush", _name));
            }
            if (!_base || !_writable || !_shared) {
                return {};
            }
#if defined(_WIN32)
            if (!win::FlushViewOfFile(_base, _mapped) || (_file != win::InvalidHandle && !win::FlushFileBuffers(_file))) {
                return fail(windows_error("flush", _name));
            }
#else
            if (::msync(_base, _mapped, MS_SYNC) != 0) {
                return fail(last_error("msync", _name));
            }
#endif
            return {};
        }

        // The file (the object) given back now; a second close does nothing
        expected<void, error> close() noexcept {
            std::lock_guard lock(_mutex);
            if (_closed.load(std::memory_order_relaxed)) {
                return {};
            }
#if defined(_WIN32)
            _closed.store(true, std::memory_order_release);
            _close_handles();
            return {};
#else
            if (_base) {
                int prot = _writable ? PROT_READ | PROT_WRITE : PROT_READ;
                (void)::mmap(_base, _mapped, prot, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);   // failing, the file's pages stay mapped until the destructor: never a hole under a slice
            }
            _closed.store(true, std::memory_order_release);
            if (_fd >= 0 && ::close(_fd) != 0) {
                return fail(last_error("close", _name));
            }
            return {};
#endif
        }

#if defined(_WIN32)
        static error windows_error(const string& op, const string& name) noexcept {
            return error(error_code(int(win::GetLastError()), std::system_category()), op, name);
        }
#endif

    private:
#if defined(_WIN32)
        SGCL_INLINE_HOT void _close_handles() noexcept {
            if (_section) {
                win::CloseHandle(_section);
            }
            if (_file != win::InvalidHandle) {
                win::CloseHandle(_file);
            }
        }
#endif

        void* const _base;         // the mapping: a page (a granule on Windows) at or before the range; null for an empty region
        const size_t _mapped;      // its length
        byte* const _data;         // the range handed out
        const size_t _size;
#if defined(_WIN32)
        const Handle _file;        // the file, InvalidHandle for shared memory or a file the region does not own
        const Handle _section;     // the file mapping object; its name lives until the last handle to it closes
#else
        const int _fd;             // the file or the shared memory object, -1 for none
#endif
        const bool _writable;
        const bool _shared;        // writes reach the file (MAP_SHARED), or stay the program's (MAP_PRIVATE)
        std::atomic<bool> _closed = {false};
        std::mutex _mutex;         // close and flush
        const string _name;        // the path or the name, for the errors
    };
}
