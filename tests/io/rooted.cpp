//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The handles of io held where a tracked word may not lie (a global, a std
// container): rooted<H> (core/rooted.h), the handle in a managed object of
// its own under a root, a copy of the handle sharing its state.
#include "tests/types.h"

#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace {
    namespace io = sgcl::io;

    // Never a conversion to the handle: *log is the handle, taken on purpose
    static_assert(!std::is_convertible_v<rooted<io::file>, io::file>);
    static_assert(!std::is_convertible_v<rooted<io::file>, const io::file&>);
    static_assert(!std::is_convertible_v<rooted<io::buffer>, io::buffer>);
    static_assert(std::is_same_v<decltype(*std::declval<const rooted<io::file>&>()), io::file&>);

    // The handles of io take part in the atomics by their word (core/detail/handle_word.h)
    static_assert(sgcl::req::handle<io::file>);
    static_assert(sgcl::req::handle<io::buffer>);
    static_assert(sgcl::req::handle<io::buffered_reader>);
    static_assert(sgcl::req::handle<io::buffered_writer>);
    static_assert(sgcl::req::handle<io::process>);

    struct IoRooted_Tests : testing::Test {
        std::string _dir;   // a std::string: the fixture lies in memory gtest allocates (The rules, 1)

        void SetUp() override {
            auto d = io::make_temp_dir({}, "sgcl-rooted-*");
            ASSERT_TRUE(d) << d.error().message();
            _dir = d->str();
        }

        void TearDown() override {
            io::remove_all(string(_dir));
        }

        string at(const char* name) const {
            return io::path::join(string(_dir), string(name));
        }
    };

    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }

    // A global: static memory, which the collector does not trace
    std::optional<rooted<io::buffer>> global_log;
}

// The form of the docs: the result of open, an expected, straight into the root
TEST_F(IoRooted_Tests, AFileOpenedIntoARoot) {
    string p = at("log.txt");
    {
        rooted<io::file> log(io::open(p, io::open_flags::write | io::open_flags::create | io::open_flags::truncate));
        EXPECT_TRUE(*log);
        ASSERT_TRUE(log->write(string("first\n")));
        ASSERT_TRUE(log->write(string("second\n")));
        ASSERT_TRUE(log->close());
    }
    expected<string, io::error> text = io::read_text(p);
    ASSERT_TRUE(text);
    EXPECT_EQ(text.value(), "first\nsecond\n");
}

// From a handle, from an expected: a copy of the handle (the same state),
// not a file made of something else; a copy of the rooted shares the object
TEST_F(IoRooted_Tests, AHandleOrAnExpectedIsCopiedIn) {
    string p = at("copy.txt");
    expected<io::file, io::error> opened = io::create(p);
    ASSERT_TRUE(opened);
    io::file f = opened;
    rooted<io::file> from_handle(f);
    rooted<io::file> from_expected(opened);
    EXPECT_TRUE(*from_handle == f);
    EXPECT_TRUE(*from_expected == f);
    rooted<io::file> copy = from_handle;
    EXPECT_EQ(copy.get(), from_handle.get());                       // the same object under both roots
    ASSERT_TRUE(copy->write(string("through the copy")));
    ASSERT_TRUE(f.close());
    EXPECT_FALSE(from_expected->write(string("closed")));           // the state is one
    expected<string, io::error> text = io::read_text(p);
    ASSERT_TRUE(text);
    EXPECT_EQ(text.value(), "through the copy");
}

TEST_F(IoRooted_Tests, AnExpectedWithAnErrorThrows) {
    EXPECT_THROW(rooted<io::file> missing(io::open(at("missing.txt"))), bad_expected_access<io::error>);
}

// Made in place from the handle's arguments
TEST_F(IoRooted_Tests, ABufferedReaderMadeInPlace) {
    string p = at("lines.txt");
    ASSERT_TRUE(io::write_file(p, string("one\ntwo\n")));
    io::file f = io::open(p);
    rooted<io::buffered_reader> in(std::in_place, f);
    std::vector<std::string> seen;
    for (auto line : in->lines()) {
        seen.emplace_back(line.data(), line.size());
    }
    EXPECT_FALSE(in->last_error());
    EXPECT_EQ(seen, (std::vector<std::string>{"one", "two"}));
}

// A stream (three words, not a handle) the same way
TEST_F(IoRooted_Tests, AStreamInARoot) {
    rooted<io::reader> src(io::reader(io::buffer("from a root")));
    settle();
    expected<string, io::error> text = src->read_all_text();
    ASSERT_TRUE(text);
    EXPECT_EQ(text.value(), "from a root");
}

// In a std container, whose memory the collector does not trace
TEST_F(IoRooted_Tests, InAStdContainer) {
    std::vector<rooted<io::buffer>> logs;
    off_frame([&] {
        for (int i = 0; i < 4; ++i) {
            logs.emplace_back(std::in_place);
            ASSERT_TRUE(logs.back()->write(string("log ") + string(std::to_string(i).c_str())));
        }
    });
    settle();
    for (size_t i = 0; i < logs.size(); ++i) {
        EXPECT_EQ(logs[i]->text(), string("log ") + string(std::to_string(i).c_str()));
    }
}

// A global root keeps the handle's state through collections; given another
// buffer, the old state is the collector's (the new one, empty, held in its
// place), and the root dropped, the box and the state too
TEST_F(IoRooted_Tests, AGlobalRootKeepsTheState) {
    settle();
    size_t states = live_objects_named("BufferState");
    size_t boxes = live_objects_named("2io6bufferE");                  // the managed object rooted keeps the handle in
    off_frame([] {
        global_log.emplace(std::in_place);
        ASSERT_TRUE((*global_log)->write(string("kept")));
    });
    settle();
    EXPECT_EQ(live_objects_named("BufferState"), states + 1);
    EXPECT_EQ(live_objects_named("2io6bufferE"), boxes + 1);
    off_frame([] {
        EXPECT_EQ((*global_log)->text(), "kept");
    });
    off_frame([] {
        **global_log = io::buffer();                                // another buffer in the handle, the box kept
    });
    settle();
    off_frame([] {
        EXPECT_EQ((*global_log)->text(), "");                       // the new one: "kept" was the old state's
    });
    EXPECT_EQ(live_objects_named("BufferState"), states + 1);       // the new state alone
    EXPECT_EQ(live_objects_named("2io6bufferE"), boxes + 1);
    global_log.reset();
    settle();
    EXPECT_EQ(live_objects_named("BufferState"), states);
    EXPECT_EQ(live_objects_named("2io6bufferE"), boxes);
}

// An atomic of a handle: load, store and exchange by identity
TEST_F(IoRooted_Tests, AnAtomicHandleByIdentity) {
    io::buffer first("first"), second("second");
    sgcl::atomic<io::buffer> current;
    current.store(first);
    EXPECT_TRUE(current.load() == first);
    EXPECT_TRUE(current.exchange(second) == first);
    EXPECT_TRUE(current.load() == second);
    EXPECT_EQ(current.load().text(), "second");
}
