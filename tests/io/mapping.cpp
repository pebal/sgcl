//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io::mapping and io::shared_memory (DESIGN 289): one managed region under
// both handles, a slice of it holding the region as its owner; the region
// held by rooted where a tracked word may not lie; two processes sharing a
// named object through io::command (this program run again, one test).
#include "tests/types.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <cstddef>
#endif

namespace {
    namespace io = sgcl::io;

    static_assert(sgcl::req::handle<io::mapping>);
    static_assert(sgcl::req::handle<io::shared_memory>);
    static_assert(sizeof(io::mapping) == sizeof(void*));
    static_assert(sizeof(io::shared_memory) == sizeof(void*));
    static_assert(std::is_same_v<decltype(std::declval<io::mapping>().data()), slice<const byte>>);
    static_assert(std::is_same_v<decltype(std::declval<io::mapping>().writable_data()), slice<byte>>);
    static_assert(std::is_same_v<decltype(std::declval<io::shared_memory>().data()), slice<byte>>);

#if defined(_WIN32)
    // The declarations of detail/win_mapping.h against windows.h: the
    // constants, the layout cast to MEMORY_BASIC_INFORMATION, the functions
    // one entity with the SDK's (a redeclaration of another type would not
    // compile beside windows.h at all)
    namespace win = sgcl::io::detail::win;
    static_assert(win::GenericRead == GENERIC_READ && win::GenericWrite == GENERIC_WRITE);
    static_assert(win::FileShareRead == FILE_SHARE_READ && win::FileShareWrite == FILE_SHARE_WRITE && win::FileShareDelete == FILE_SHARE_DELETE);
    static_assert(win::OpenExisting == OPEN_EXISTING && win::FileAttributeNormal == FILE_ATTRIBUTE_NORMAL);
    static_assert(win::PageReadonly == PAGE_READONLY && win::PageReadwrite == PAGE_READWRITE && win::PageWritecopy == PAGE_WRITECOPY);
    static_assert(win::FileMapCopy == FILE_MAP_COPY && win::FileMapWrite == FILE_MAP_WRITE && win::FileMapRead == FILE_MAP_READ && win::FileMapAllAccess == FILE_MAP_ALL_ACCESS);
    static_assert(win::ErrorAlreadyExists == ERROR_ALREADY_EXISTS && win::CodePageUtf8 == CP_UTF8);
    static_assert(sizeof(win::MemoryInfo) == sizeof(MEMORY_BASIC_INFORMATION));
    static_assert(offsetof(win::MemoryInfo, region_size) == offsetof(MEMORY_BASIC_INFORMATION, RegionSize));
    static_assert(sizeof(long long) == sizeof(LARGE_INTEGER));
    static_assert(std::is_same_v<decltype(&win::CreateFileMappingW), decltype(&::CreateFileMappingW)>);
    static_assert(std::is_same_v<decltype(&win::MapViewOfFile), decltype(&::MapViewOfFile)>);
    static_assert(std::is_same_v<decltype(&win::VirtualQuery), decltype(&::VirtualQuery)>);

    TEST(IoMappingWindows, TheAllocationGranularity) {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        EXPECT_EQ(info.dwAllocationGranularity, win::AllocationGranularity);
        EXPECT_EQ(&win::CloseHandle, &::CloseHandle);
    }
#endif

    struct IoMapping_Tests : testing::Test {
        std::string _dir;   // a std::string: the fixture lies in memory gtest allocates (The rules, 1)

        void SetUp() override {
            auto d = io::make_temp_dir({}, "sgcl-map-*");
            ASSERT_TRUE(d) << d.error().message();
            _dir = d->str();
        }

        void TearDown() override {
            io::remove_all(string(_dir));
        }

        string at(const char* name) const {
            return io::path::join(string(_dir), string(name));
        }

        // A file of n bytes, byte i being i % 251
        string numbered(const char* name, size_t n) const {
            std::vector<byte> b(n);
            for (size_t i = 0; i < n; ++i) {
                b[i] = byte(i % 251);
            }
            string p = at(name);
            EXPECT_TRUE(io::write_file(p, slice<const byte>(b.data(), b.size())));
            return p;
        }
    };

    std::string text_of(const slice<const byte>& s) {
        return std::string(reinterpret_cast<const char*>(s.data()), s.size());
    }

    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }

    size_t regions() {
        return live_objects_named("MappedRegion");
    }

    // A name of this process's own, short enough for macOS (30 characters)
    string shared_name(const char* what) {
        return string(("sgcl-" + std::string(what) + "-" + std::to_string(io::pid())).c_str());
    }
}

TEST_F(IoMapping_Tests, ReadsTheWholeFile) {
    string p = at("a.txt");
    ASSERT_TRUE(io::write_file(p, string("hello, mapping")));
    auto m = io::map(p);
    ASSERT_TRUE(m) << m.error().message();
    EXPECT_EQ(m->size(), 14u);
    EXPECT_EQ(text_of(m->data()), "hello, mapping");
    EXPECT_FALSE(m->is_closed());
    EXPECT_TRUE(m->flush());                                  // read only: nothing to do
}

// Written through the slice: the file has it after flush, and after close
TEST_F(IoMapping_Tests, WriteFlushAndClose) {
    string p = at("w.txt");
    ASSERT_TRUE(io::write_file(p, string("aaaaaaaaaa")));
    io::mapping m = io::map(p, io::map_options{.writable = true});
    slice<byte> d = m.writable_data();
    ASSERT_EQ(d.size(), 10u);
    d[0] = byte('X');
    ASSERT_TRUE(m.flush());
    EXPECT_EQ(io::read_text(p).value(), "Xaaaaaaaaa");
    d[9] = byte('Z');
    ASSERT_TRUE(m.close());
    EXPECT_EQ(io::read_text(p).value(), "XaaaaaaaaZ");
}

// shared = false: a private copy on write, the file untouched
TEST_F(IoMapping_Tests, APrivateMappingLeavesTheFile) {
    string p = at("p.txt");
    ASSERT_TRUE(io::write_file(p, string("original")));
    io::mapping m = io::map(p, io::map_options{.writable = true, .shared = false});
    m.writable_data()[0] = byte('O');
    EXPECT_EQ(text_of(m.data()), "Original");
    ASSERT_TRUE(m.flush());
    ASSERT_TRUE(m.close());
    EXPECT_EQ(io::read_text(p).value(), "original");
}

// An offset inside a page: aligned down inside, the slice from the byte asked
TEST_F(IoMapping_Tests, AnOffsetThatIsNotPageAligned) {
    size_t page = size_t(::sysconf(_SC_PAGESIZE));
    string p = numbered("n.bin", 3 * page + 100);
    uint64_t offset = page + 123;
    io::mapping m = io::map(p, io::map_options{.offset = offset, .length = 50});
    ASSERT_EQ(m.size(), 50u);
    for (size_t i = 0; i < 50; ++i) {
        ASSERT_EQ(m.data()[i], byte((offset + i) % 251)) << i;
    }
    io::mapping rest = io::map(p, io::map_options{.offset = offset});      // length 0: to the end
    EXPECT_EQ(rest.size(), 3 * page + 100 - offset);
    EXPECT_EQ(rest.data()[rest.size() - 1], byte((3 * page + 99) % 251));
    io::mapping writing = io::map(p, io::map_options{.writable = true, .offset = 7, .length = 1});
    writing.writable_data()[0] = byte(0xEE);
    ASSERT_TRUE(writing.close());
    EXPECT_EQ(io::read_file(p).value()[7], byte(0xEE));
}

// An empty file maps to an empty region; a file grown after the mapping:
// the mapping keeps its size, a new one sees the rest
TEST_F(IoMapping_Tests, AnEmptyFileAndAFileThatGrows) {
    string empty = at("empty");
    ASSERT_TRUE(io::write_file(empty, string("")));
    io::mapping e = io::map(empty);
    EXPECT_EQ(e.size(), 0u);
    EXPECT_TRUE(e.data().empty());
    io::mapping ew = io::map(empty, io::map_options{.writable = true});
    EXPECT_TRUE(ew.writable_data().empty());
    EXPECT_TRUE(ew.flush());
    EXPECT_TRUE(ew.close());

    string p = at("grows.txt");
    ASSERT_TRUE(io::write_file(p, string("start")));
    io::mapping m = io::map(p);
    ASSERT_TRUE(io::append_file(p, string(" and more")));
    EXPECT_EQ(m.size(), 5u);
    EXPECT_EQ(text_of(m.data()), "start");
    EXPECT_EQ(text_of(io::map(p)->data()), "start and more");
}

// A mapping of an open file holds a descriptor of its own
TEST_F(IoMapping_Tests, AnOpenFile) {
    string p = at("f.txt");
    ASSERT_TRUE(io::write_file(p, string("through a file")));
    io::file f = io::open(p);
    io::mapping m = io::map(f, io::map_options{.offset = 8});
    ASSERT_TRUE(f.close());
    EXPECT_EQ(text_of(m.data()), "a file");
    auto closed = io::map(f);
    ASSERT_FALSE(closed);
    EXPECT_TRUE(closed.error().is_closed());
}

// close(): the file given back, a slice taken before reads zeros, not a hole
TEST_F(IoMapping_Tests, CloseLeavesNoHole) {
    string p = at("c.txt");
    ASSERT_TRUE(io::write_file(p, string("closing")));
    io::mapping m = io::map(p, io::map_options{.writable = true});
    slice<byte> before = m.writable_data();
    ASSERT_TRUE(m.close());
    EXPECT_TRUE(m.is_closed());
    EXPECT_EQ(m.size(), 0u);
    EXPECT_TRUE(m.data().empty());
    EXPECT_EQ(before[0], byte(0));
    before[1] = byte('x');                                     // anonymous memory now: the file keeps its bytes
    EXPECT_EQ(io::read_text(p).value(), "closing");
    auto again = m.flush();
    ASSERT_FALSE(again);
    EXPECT_TRUE(again.error().is_closed());
    EXPECT_TRUE(m.close());                                    // twice: nothing
}

// A slice kept after the last handle: the region is its owner
TEST_F(IoMapping_Tests, ASliceOutlivesTheHandle) {
    string p = at("s.txt");
    ASSERT_TRUE(io::write_file(p, string("kept by the slice")));
    settle();
    size_t before = regions();
    std::optional<rooted<slice<const byte>>> kept;
    off_frame([&] {
        io::mapping m = io::map(p);
        kept.emplace(m.data().subslice(8));
    });
    settle();
    EXPECT_EQ(regions(), before + 1);
    EXPECT_EQ(text_of(**kept), "the slice");
    kept.reset();
    settle();
    EXPECT_EQ(regions(), before);
}

TEST_F(IoMapping_Tests, Errors) {
    auto missing = io::map(at("missing"));
    ASSERT_FALSE(missing);
    EXPECT_TRUE(missing.error().is_not_found());

    string p = at("short.txt");
    ASSERT_TRUE(io::write_file(p, string("0123456789")));
    for (bool writable : {false, true}) {
        // a range past the end: an error, the file never extended
        auto past = io::map(p, io::map_options{.writable = writable, .offset = 4, .length = 7});
        ASSERT_FALSE(past);
        EXPECT_EQ(past.error().code(), std::errc::invalid_argument);
        EXPECT_FALSE(io::map(p, io::map_options{.writable = writable, .offset = 11}));
        EXPECT_TRUE(io::map(p, io::map_options{.writable = writable, .offset = 10}));   // the end itself: empty
    }
    EXPECT_EQ(io::stat(p)->size, 10u);

    // a writable shared mapping of a file the program may not write
    string ro = at("ro.txt");
    ASSERT_TRUE(io::write_file(ro, string("read only")));
    ASSERT_TRUE(io::chmod(ro, io::permissions(0444)));
    auto denied = io::map(ro, io::map_options{.writable = true});
    ASSERT_FALSE(denied);
    EXPECT_TRUE(denied.error().is_permission()) << denied.error().message();
    EXPECT_TRUE(io::map(ro, io::map_options{.writable = true, .shared = false}));   // a private copy needs no write
    auto opened = io::open(p);
    auto through_file = io::map(*opened, io::map_options{.writable = true});
    ASSERT_FALSE(through_file);
    EXPECT_TRUE(through_file.error().is_permission()) << through_file.error().message();
}

TEST(IoSharedMemory_Tests, CreateOpenRemove) {
    string name = shared_name("com");
    (void)io::shared_memory::remove(name);
    auto a = io::shared_memory::create(name, 100);
    ASSERT_TRUE(a) << a.error().message();
    EXPECT_EQ(a->size(), 100u);
    EXPECT_EQ(a->data()[99], byte(0));                         // zeros
    auto b = io::shared_memory::open(name);
    ASSERT_TRUE(b) << b.error().message();
    EXPECT_GE(b->size(), 100u);                                // macOS rounds the object up to a page
    EXPECT_FALSE(*a == *b);
    a->data()[5] = byte(42);
    EXPECT_EQ(b->data()[5], byte(42));                         // one object, two regions

    auto taken = io::shared_memory::create(name, 100);
    ASSERT_FALSE(taken);
    EXPECT_TRUE(taken.error().is_exists());

    ASSERT_TRUE(io::shared_memory::remove(name));
    EXPECT_EQ(b->data()[5], byte(42));                         // the mappings keep the object
    auto gone = io::shared_memory::open(name);
    ASSERT_FALSE(gone);
    EXPECT_TRUE(gone.error().is_not_found());
    auto again = io::shared_memory::remove(name);
    ASSERT_FALSE(again);
    EXPECT_TRUE(again.error().is_not_found());
}

TEST(IoSharedMemory_Tests, Errors) {
    for (const char* bad : {"", "a/b", "/leading", "back\\slash"}) {
        auto c = io::shared_memory::create(string(bad), 64);
        ASSERT_FALSE(c) << bad;
        EXPECT_EQ(c.error().code(), io::errc::invalid_path);
        EXPECT_EQ(io::shared_memory::open(string(bad)).error().code(), io::errc::invalid_path);
        EXPECT_EQ(io::shared_memory::remove(string(bad)).error().code(), io::errc::invalid_path);
    }
    string name = shared_name("zero");
    auto zero = io::shared_memory::create(name, 0);            // a size too small: nothing made
    ASSERT_FALSE(zero);
    EXPECT_EQ(zero.error().code(), std::errc::invalid_argument);
    EXPECT_FALSE(io::shared_memory::open(name));
}

// close(): the region given back, an old slice reads zeros, the object
// and its name kept until remove
TEST(IoSharedMemory_Tests, Close) {
    string name = shared_name("close");
    (void)io::shared_memory::remove(name);
    io::shared_memory s = io::shared_memory::create(name, 64);
    slice<byte> before = s.data();
    before[0] = byte(7);
    io::shared_memory copy = s;
    ASSERT_TRUE(s.close());
    EXPECT_TRUE(copy.is_closed());                             // one region under both handles
    EXPECT_EQ(s.size(), 0u);
    EXPECT_TRUE(s.data().empty());
    EXPECT_EQ(before[0], byte(0));                             // zeros, not a hole
    before[1] = byte(9);                                       // anonymous memory now: the object keeps its bytes
    EXPECT_TRUE(s.close());                                    // twice: nothing
    io::shared_memory again = io::shared_memory::open(name);   // the name kept
    EXPECT_EQ(again.data()[0], byte(7));
    EXPECT_EQ(again.data()[1], byte(0));
    ASSERT_TRUE(io::shared_memory::remove(name));
}

// The other process: this program again, this test alone (disabled in the
// suite), the name in the environment
TEST(IoSharedMemory_Tests, DISABLED_ChildWrites) {
    auto name = io::getenv("SGCL_SHARED_MEMORY_TEST");
    ASSERT_TRUE(name);
    io::shared_memory s = io::shared_memory::open(*name);
    const char text[] = "from the child";
    for (size_t i = 0; i + 1 < sizeof(text); ++i) {
        s.data()[i] = byte(text[i]);
    }
}

TEST(IoSharedMemory_Tests, TwoProcesses) {
    string name = shared_name("two");
    (void)io::shared_memory::remove(name);
    io::shared_memory s = io::shared_memory::create(name, 4096);
    ASSERT_TRUE(io::setenv("SGCL_SHARED_MEMORY_TEST", name));
    io::command child(io::executable().value(), "--gtest_filter=IoSharedMemory_Tests.DISABLED_ChildWrites", "--gtest_also_run_disabled_tests");
    auto out = child.combined_output();
    ASSERT_TRUE(io::unsetenv("SGCL_SHARED_MEMORY_TEST"));
    ASSERT_TRUE(out) << out.error().message();
    EXPECT_EQ(text_of(s.data().first(14)), "from the child") << out->str();
    ASSERT_TRUE(io::shared_memory::remove(name));
}

// The handles and a slice of the region held where a tracked word may not
// lie, as rooted<io::file> is (rooted.cpp)
namespace {
    std::optional<rooted<io::mapping>> global_mapping;
}

TEST_F(IoMapping_Tests, RootedHandlesAndSlices) {
    string p = at("r.txt");
    ASSERT_TRUE(io::write_file(p, string("rooted region")));
    string name = shared_name("root");
    (void)io::shared_memory::remove(name);
    settle();
    size_t before = regions();

    // a global root, a std container of roots
    std::vector<rooted<io::shared_memory>> shared;
    off_frame([&] {
        global_mapping.emplace(io::map(p));
        shared.emplace_back(io::shared_memory::create(name, 64));
        shared.emplace_back(io::shared_memory::open(name));
        (*shared[0]).data()[0] = byte('s');
    });
    settle();
    EXPECT_EQ(regions(), before + 3);
    off_frame([&] {
        EXPECT_EQ(text_of((*global_mapping)->data()), "rooted region");
        EXPECT_EQ(shared[1]->data()[0], byte('s'));
    });

    // a slice of the region held by a root alone
    std::optional<rooted<slice<byte>>> kept;
    off_frame([&] {
        kept.emplace((*shared[0]).data());
    });
    shared.clear();
    global_mapping.reset();
    settle();
    EXPECT_EQ(regions(), before + 1);                          // the first shared region, through the slice
    off_frame([&] {
        EXPECT_EQ((**kept)[0], byte('s'));
        (**kept)[1] = byte('t');
    });

    // released: the region too
    kept.reset();
    settle();
    EXPECT_EQ(regions(), before);
    ASSERT_TRUE(io::shared_memory::remove(name));
}

// Copies of a handle share the region; an atomic of one by identity
TEST_F(IoMapping_Tests, CopiesAndAtomics) {
    string p = at("h.txt");
    ASSERT_TRUE(io::write_file(p, string("shared")));
    io::mapping a = io::map(p), b = a;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == io::map(p).value());
    ASSERT_TRUE(b.close());
    EXPECT_TRUE(a.is_closed());
    io::mapping empty;
    EXPECT_FALSE(empty);
    sgcl::atomic<io::mapping> current;
    current.store(a);
    EXPECT_TRUE(current.load() == a);
}
