//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/handle_word.h"
#include "detail/mapped_region.h"
#include "file.h"

#include <cassert>
#include <cstdint>

#if !defined(_WIN32)
#include <fcntl.h>
#endif

namespace sgcl::io {
    // How a file is mapped (DESIGN 289): read only by default; `writable`
    // with `shared` (the default) writes to the file itself, as Go's mmap
    // packages and Python's ACCESS_WRITE do; `writable` without `shared`
    // is a private copy on write, the file untouched. The range starts at
    // `offset` (any byte: the mapping is aligned down to a page inside)
    // and spans `length` bytes, 0 for the rest of the file; a range past
    // the end of the file is an error, the file never extended (truncate
    // it first).
    struct map_options {
        bool writable = false;
        bool shared = true;
        uint64_t offset = 0;
        uint64_t length = 0;
    };

    class mapping;

    namespace detail {
        struct MappingAccess;
    }

    // A file mapped into memory: a handle of one word, a tracked_ptr to
    // the region inside (detail::MappedRegion), whose copies share it.
    // data() is the file's bytes as a slice<const byte>, writable_data()
    // as a slice<byte> for a writable mapping; the slice holds the region
    // as its owner, so a slice kept after the last handle is gone still
    // reads the mapping, which is unmapped when nothing holds it any more
    // (by the destructor on the collector's thread) or given back by
    // close(). Made by io::map; a default-constructed one holds no
    // mapping (`!m`), and an operation on it is a contract violation.
    class mapping final {
    public:
        mapping() noexcept = default;

        // The mapped bytes, read only; empty once closed
        slice<const byte> data() const noexcept {
            auto& r = _get();
            if (r.size() == 0) {
                return {};
            }
            return slice<const byte>(_region, r.begin(), r.begin() + r.size(), sgcl::detail::OutsideOwner{});
        }

        // The mapped bytes to write into: a mapping made writable (a
        // contract violation on one read only, an empty slice without the
        // debug check); empty once closed
        slice<byte> writable_data() const noexcept {
            auto& r = _get();
            assert(r.writable() && "writable_data() of a read-only io::mapping");
            if (!r.writable() || r.size() == 0) {
                return {};
            }
            return slice<byte>(_region, r.begin(), r.begin() + r.size(), sgcl::detail::OutsideOwner{});
        }

        // The length of the range, 0 once closed
        size_t size() const noexcept {
            return _get().size();
        }

        // A writable shared mapping's writes given to the file and waited
        // for (msync MS_SYNC); nothing to do for any other mapping
        expected<void, error> flush() const noexcept {
            return _get().flush();
        }

        // The file given back now, the mapping's range left as zeros for a
        // slice taken before; data() is empty after it. The destructor
        // unmaps a mapping nobody closed
        expected<void, error> close() const noexcept {
            return _get().close();
        }

        bool is_closed() const noexcept {
            return _get().closed();
        }

        // Whether this handle holds a mapping
        explicit operator bool() const noexcept {
            return (bool)_region;
        }

        // The same mapping: the same region
        friend bool operator==(const mapping& a, const mapping& b) noexcept {
            return a._region == b._region;
        }

    private:
        friend struct detail::MappingAccess;

        explicit mapping(tracked_ptr<detail::MappedRegion> region) noexcept
        : _region(std::move(region)) {
        }

        detail::MappedRegion& _get() const noexcept {
            assert(_region && "an empty io::mapping");
            return *_region;
        }

        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        mapping(sgcl::detail::FromWord, const tracked_ptr<detail::MappedRegion>& w) noexcept
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
        // The handle made over a region, for io::map
        struct MappingAccess {
            static mapping make(tracked_ptr<MappedRegion> region) noexcept {
                return mapping(std::move(region));
            }
        };

        // The range [offset, offset + length) of a file of `file_size`
        // bytes, length 0 for the rest: its length, or an error for a
        // range past the end
        inline expected<uint64_t, error> map_range(uint64_t file_size, const map_options& o, const string& name) noexcept {
            uint64_t length = o.length ? o.length : (o.offset <= file_size ? file_size - o.offset : 0);
            if (o.offset > file_size || length > file_size - o.offset) {
                return fail(error(error_code(EINVAL, std::generic_category()), "map", name));   // a range past the end: an error, the file never extended
            }
            if (length > uint64_t(SIZE_MAX)) {
                return fail(error(error_code(EFBIG, std::generic_category()), "map", name));
            }
            return length;
        }

#if defined(_WIN32)
        // The file (owned when `owns`) mapped; the handle closed on failure
        inline expected<void, error> map_handle(win::Handle file, bool owns, const map_options& o, const string& name, tracked_ptr<MappedRegion>& out) noexcept {
            auto give_back = [&] {
                if (owns) {
                    win::CloseHandle(file);
                }
            };
            long long file_size = 0;
            if (!win::GetFileSizeEx(file, reinterpret_cast<::_LARGE_INTEGER*>(&file_size))) {
                auto e = MappedRegion::windows_error("map", name);
                give_back();
                return fail(e);
            }
            auto length = map_range(uint64_t(file_size), o, name);
            if (!length) {
                give_back();
                return fail(length);
            }
            win::Handle kept = owns ? file : win::InvalidHandle;
            if (*length == 0) {
                out = make_tracked<MappedRegion>(nullptr, 0, nullptr, 0, kept, nullptr, o.writable, o.shared, name);
                return {};
            }
            uint64_t aligned = o.offset & ~uint64_t(win::AllocationGranularity - 1);
            size_t mapped = size_t(o.offset - aligned + *length);
            win::Dword protect = !o.writable ? win::PageReadonly : o.shared ? win::PageReadwrite : win::PageWritecopy;
            win::Dword access = !o.writable ? win::FileMapRead : o.shared ? win::FileMapWrite : win::FileMapCopy;
            win::Handle section = win::CreateFileMappingW(file, nullptr, protect, 0, 0, nullptr);
            if (!section) {
                auto e = MappedRegion::windows_error("map", name);
                give_back();
                return fail(e);
            }
            void* base = win::MapViewOfFile(section, access, win::Dword(aligned >> 32), win::Dword(aligned), mapped);
            if (!base) {
                auto e = MappedRegion::windows_error("map", name);
                win::CloseHandle(section);
                give_back();
                return fail(e);
            }
            byte* data = static_cast<byte*>(base) + (o.offset - aligned);
            out = make_tracked<MappedRegion>(base, mapped, data, size_t(*length), kept, section, o.writable, o.shared, name);
            return {};
        }
#else
        // The descriptor (the region's from now on, closed on failure) mapped
        inline expected<tracked_ptr<MappedRegion>, error> map_fd(int fd, const map_options& o, const string& name) noexcept {
            struct ::stat st;
            if (::fstat(fd, &st) != 0) {
                auto e = last_error("map", name);
                ::close(fd);
                return fail(e);
            }
            auto length = map_range(uint64_t(st.st_size), o, name);
            if (!length) {
                ::close(fd);
                return fail(length);
            }
            if (*length == 0) {
                return make_tracked<MappedRegion>(nullptr, 0, nullptr, 0, fd, o.writable, o.shared, name);
            }
            uint64_t page = uint64_t(::sysconf(_SC_PAGESIZE));
            uint64_t aligned = o.offset & ~(page - 1);
            size_t mapped = size_t(o.offset - aligned + *length);
            int prot = o.writable ? PROT_READ | PROT_WRITE : PROT_READ;
            int flags = o.writable && o.shared ? MAP_SHARED : MAP_PRIVATE;
            void* base = ::mmap(nullptr, mapped, prot, flags, fd, off_t(aligned));
            if (base == MAP_FAILED) {
                auto e = last_error("mmap", name);
                ::close(fd);
                return fail(e);
            }
            byte* data = static_cast<byte*>(base) + (o.offset - aligned);
            return make_tracked<MappedRegion>(base, mapped, data, size_t(*length), fd, o.writable, o.shared, name);
        }
#endif
    }

    // The file at `path` mapped: read only by default, the whole file;
    // map_options for a writable mapping, a private one, or a range. The
    // error of the open (is_not_found(), is_permission(): a writable
    // shared mapping of a file the program may not write), of a range past
    // the end (EINVAL), or of the mmap
    inline expected<mapping, error> map(const string& path, const map_options& options) noexcept {
#if defined(_WIN32)
        auto name = win::wide<std::wstring>(path.data(), int(path.size()));
        win::Dword access = win::GenericRead | (options.writable && options.shared ? win::GenericWrite : 0);
        win::Handle h = win::CreateFileW(name.c_str(), access, win::FileShareRead | win::FileShareWrite | win::FileShareDelete, nullptr, win::OpenExisting, win::FileAttributeNormal, nullptr);
        if (h == win::InvalidHandle) {
            return detail::fail(detail::MappedRegion::windows_error("open", path));
        }
        tracked_ptr<detail::MappedRegion> r;
        if (auto m = detail::map_handle(h, true, options, path, r); !m) {
            return detail::fail(m);
        }
        return detail::MappingAccess::make(std::move(r));
#else
        int f = O_CLOEXEC | (options.writable && options.shared ? O_RDWR : O_RDONLY);
        int fd;
        do {
            fd = ::open(path.c_str(), f);
        } while (fd < 0 && errno == EINTR);
        if (fd < 0) {
            return detail::fail(last_error("open", path));
        }
        auto r = detail::map_fd(fd, options, path);
        if (!r) {
            return detail::fail(r);
        }
        return detail::MappingAccess::make(std::move(*r));
#endif
    }

    // The whole file at `path`, read only
    inline expected<mapping, error> map(const string& path) noexcept {
        return map(path, map_options{});
    }

    // An open file mapped: the mapping holds a descriptor of its own (a
    // dup), so the file may be closed after; a writable shared mapping
    // needs a file opened for reading and writing (EACCES otherwise)
    inline expected<mapping, error> map(const file& f, const map_options& options = {}) noexcept {
#if defined(_WIN32)
        long long h = win::_get_osfhandle(f.fd());
        if (h == -1) {
            return detail::fail(error(errc::closed, "map", f.path()));
        }
        tracked_ptr<detail::MappedRegion> r;
        if (auto m = detail::map_handle(reinterpret_cast<win::Handle>(h), false, options, f.path(), r); !m) {
            return detail::fail(m);
        }
        return detail::MappingAccess::make(std::move(r));
#else
        int fd = f.fd() < 0 ? -1 : ::fcntl(f.fd(), F_DUPFD_CLOEXEC, 0);
        if (fd < 0) {
            return detail::fail(f.fd() < 0 ? error(errc::closed, "map", f.path()) : last_error("map", f.path()));
        }
        auto r = detail::map_fd(fd, options, f.path());
        if (!r) {
            return detail::fail(r);
        }
        return detail::MappingAccess::make(std::move(*r));
#endif
    }
}
