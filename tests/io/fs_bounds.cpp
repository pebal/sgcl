//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// fs.h, mapping.h, shared_memory.h, os.h and path.h at their boundaries
// (DESIGN 408): empty paths, trailing separators, a path that is a file
// where a directory is wanted and the other way round, entries that vanish
// or change type during a walk, a file copied onto itself, times before
// 1970, a mapping and a region of size 0, variables with empty names and
// values, patterns malformed after a separator.
#include "tests/types.h"

using namespace sgcl::async;

#include <chrono>
#include <fcntl.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

namespace {
    using namespace sgcl::io;
    namespace io = sgcl::io;

    struct IoFsBounds_Tests : testing::Test {
        std::string _dir;

        void SetUp() override {
            auto d = make_temp_dir({}, "sgcl-fs-bounds-*");
            ASSERT_TRUE(d) << d.error().message();
            _dir = d->str();
        }

        void TearDown() override {
            (void)io::chmod(path::join(string(_dir), string("locked")), permissions(0700));
            io::remove_all(string(_dir));
        }

        string at(const char* name) const {
            return path::join(string(_dir), string(name));
        }
    };

    std::string text_of(const slice<const byte>& b) {
        return std::string(reinterpret_cast<const char*>(b.data()), b.size());
    }
}

// Empty paths: nothing there, every call an error or false; mkdir_all of
// "" is "." after cleaning, there already
TEST_F(IoFsBounds_Tests, EmptyPaths) {
    EXPECT_TRUE(error_of(stat("")).is_not_found());
    EXPECT_TRUE(error_of(lstat("")).is_not_found());
    EXPECT_FALSE(exists(""));
    EXPECT_FALSE(is_directory(""));
    EXPECT_FALSE(is_regular(""));
    EXPECT_TRUE(error_of(mkdir("")).is_not_found());
    EXPECT_TRUE(mkdir_all(""));
    EXPECT_TRUE(error_of(io::remove("")).is_not_found());
    EXPECT_TRUE(remove_all(""));
    EXPECT_TRUE(error_of(rename("", at("x"))).is_not_found());
    EXPECT_FALSE(copy_file("", at("x")));
    EXPECT_FALSE(read_link(""));
    EXPECT_FALSE(chmod("", permissions(0600)));
    EXPECT_FALSE(set_modified("", file_time()));
    EXPECT_TRUE(error_of(read_dir("")).is_not_found());
    EXPECT_TRUE(error_of(walk_dir("", [](const directory_entry&, const optional<io::error>&) { return walk_action::next; })).is_not_found());
    EXPECT_TRUE(error_of(io::map("")).is_not_found());
    EXPECT_FALSE(exists(at("x")));
}

// Trailing separators: a directory with one is the directory; a file with
// one is ENOTDIR; mkdir_all and read_dir clean it; the name of a stat is
// the last element
TEST_F(IoFsBounds_Tests, TrailingSeparators) {
    ASSERT_TRUE(mkdir_all(at("a") + "/b/"));
    EXPECT_TRUE(is_directory(at("a/b")));
    EXPECT_TRUE(mkdir_all(at("a") + "//b///"));
    string a_slash = at("a") + "/";
    EXPECT_EQ(value_of(stat(a_slash)).name, "a");
    EXPECT_EQ(value_of(stat("/")).name, "/");
    auto entries = value_of(read_dir(a_slash));
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].path, at("a/b"));
    ASSERT_TRUE(write_file(at("f"), "x"));
    string f_slash = at("f") + "/";
    EXPECT_EQ(error_of(stat(f_slash)).code(), std::errc::not_a_directory);
    EXPECT_FALSE(exists(f_slash));
    EXPECT_EQ(error_of(read_dir(at("f"))).code(), std::errc::not_a_directory);
    EXPECT_EQ(error_of(io::map(f_slash)).code(), std::errc::not_a_directory);
    EXPECT_TRUE(io::remove(at("a/b") + "/"));
    EXPECT_FALSE(exists(at("a/b")));
}

// mkdir_all where a file is in the way: at the end, in the middle
TEST_F(IoFsBounds_Tests, MkdirAllOverAFile) {
    ASSERT_TRUE(write_file(at("file"), "x"));
    EXPECT_EQ(error_of(mkdir_all(at("file"))).code(), std::errc::not_a_directory);
    EXPECT_EQ(error_of(mkdir_all(at("file/sub"))).code(), std::errc::not_a_directory);
    EXPECT_EQ(error_of(mkdir(at("file"))).code(), std::errc::file_exists);
    ASSERT_TRUE(symlink(at("real"), at("link")));
    ASSERT_TRUE(mkdir(at("real")));
    EXPECT_TRUE(mkdir_all(at("link")));   // a symlink to a directory is a directory there
}

// remove_all of a path whose last element is "." or "..": refused before
// anything is removed (rmdir refuses them, and Go's RemoveAll too), the
// contents left
TEST_F(IoFsBounds_Tests, RemoveAllOfDotRemovesNothing) {
    ASSERT_TRUE(mkdir_all(at("keep/inner")));
    ASSERT_TRUE(write_file(at("keep/inner/file"), "x"));
    string dot = at("keep") + "/.";
    string dotdot = at("keep/inner") + "/..";
    EXPECT_EQ(error_of(remove_all(dot)).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(remove_all(dotdot)).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(remove_all(dot + "/")).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(remove_all(".")).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(spawn(async_remove_all(dot)).wait()).code(), std::errc::invalid_argument);
    EXPECT_EQ(value_of(read_text(at("keep/inner/file"))), "x");
    EXPECT_TRUE(remove_all(at("keep")));
    EXPECT_FALSE(exists(at("keep")));
    EXPECT_TRUE(remove_all(at("keep")));   // nothing there: no error
}

// remove, rename and copy_file at their bounds: a directory with entries,
// a file onto itself, a directory where a file is wanted
TEST_F(IoFsBounds_Tests, RemoveRenameCopyAtTheirBounds) {
    ASSERT_TRUE(mkdir_all(at("d/e")));
    EXPECT_EQ(error_of(io::remove(at("d"))).code(), std::errc::directory_not_empty);
    ASSERT_TRUE(write_file(at("f"), "content"));
    EXPECT_TRUE(rename(at("f"), at("f")));   // onto itself: nothing
    EXPECT_EQ(value_of(read_text(at("f"))), "content");
    EXPECT_FALSE(rename(at("d"), at("d/e/inside")));   // into itself: EINVAL
    EXPECT_FALSE(copy_file(at("f"), at("f")));          // onto itself: refused, the file intact
    EXPECT_EQ(value_of(read_text(at("f"))), "content");
    EXPECT_FALSE(copy_file(at("d"), at("g")));          // a directory is no regular file
    EXPECT_FALSE(copy_file(at("f"), at("d")));          // nor is the target
    EXPECT_FALSE(copy_file(at("missing"), at("g")));
    EXPECT_FALSE(exists(at("g")));
    ASSERT_TRUE(write_file(at("empty"), ""));
    ASSERT_TRUE(copy_file(at("empty"), at("g")));
    EXPECT_EQ(value_of(stat(at("g"))).size, 0u);
    EXPECT_EQ(error_of(read_link(at("f"))).code(), std::errc::invalid_argument);
}

// Times before 1970 and a fraction of a second: the time read back is the
// time set (a negative remainder was handed to the system as is: EINVAL)
TEST_F(IoFsBounds_Tests, ModifiedTimesBeforeTheEpoch) {
    ASSERT_TRUE(write_file(at("t"), "x"));
    using std::chrono::nanoseconds;
    for (int64_t ns : {int64_t(0), int64_t(-1), int64_t(-500'000'000), int64_t(-1'500'000'000), int64_t(-86'400'000'000'000), int64_t(1'250'000'000)}) {
        file_time t{nanoseconds(ns)};
        ASSERT_TRUE(set_modified(at("t"), t)) << ns;
        auto info = value_of(stat(at("t")));
        EXPECT_EQ(info.modified.time_since_epoch().count(), ns);
    }
}

// A walk whose entries change under it: a directory removed before the
// walk enters it is reported with its error, one made a file likewise, and
// the walk goes on; a root that is a file, a symlink to a directory, empty
TEST_F(IoFsBounds_Tests, AWalkWhoseEntriesChange) {
    ASSERT_TRUE(mkdir_all(at("root/a")));
    ASSERT_TRUE(mkdir_all(at("root/b")));
    ASSERT_TRUE(write_file(at("root/b/x"), "x"));
    ASSERT_TRUE(write_file(at("root/c"), "c"));
    std::vector<std::string> seen;
    string a = at("root/a"), b = at("root/b");
    auto r = walk_dir(at("root"), [&](const directory_entry& e, const optional<io::error>& err) {
        seen.push_back(std::string(e.name.view()) + (err ? (err->is_not_found() ? "!gone" : "!err") : ""));
        if (!err && e.path == a) {
            (void)io::remove(a);
        }
        if (!err && e.path == b) {
            (void)remove_all(b);
            (void)write_file(b, "now a file");
        }
        return walk_action::next;
    });
    ASSERT_TRUE(r);
    std::vector<std::string> want = {"a", "a!gone", "b", "b!err", "c"};
    EXPECT_EQ(seen, want);

    auto async_seen = std::make_shared<std::vector<std::string>>();
    ASSERT_TRUE(mkdir(at("root/a")));
    auto ar = spawn(async_walk_dir(at("root"), [async_seen, a](const directory_entry& e, const optional<io::error>& err) {
        async_seen->push_back(std::string(e.name.view()) + (err ? "!err" : ""));
        if (!err && e.path == a) {
            (void)io::remove(a);
        }
        return walk_action::next;
    })).wait();
    ASSERT_TRUE(ar);
    std::vector<std::string> async_want = {"a", "a!err", "b", "c"};
    EXPECT_EQ(*async_seen, async_want);

    auto none = [](const directory_entry&, const optional<io::error>&) { return walk_action::next; };
    EXPECT_EQ(error_of(walk_dir(at("root/c"), none)).code(), std::errc::not_a_directory);
    ASSERT_TRUE(symlink(at("root"), at("to_root")));
    EXPECT_EQ(error_of(walk_dir(at("to_root"), none)).code(), std::errc::not_a_directory);   // a symlink is not followed
    ASSERT_TRUE(mkdir(at("empty")));
    int calls = 0;
    EXPECT_TRUE(walk_dir(at("empty"), [&](const directory_entry&, const optional<io::error>&) { ++calls; return walk_action::next; }));
    EXPECT_EQ(calls, 0);
    EXPECT_TRUE(walk_dir(at("empty") + "/", none));
}

// A mapping of size 0 at every member, and the ranges at their bounds
TEST_F(IoFsBounds_Tests, AMappingOfSizeZero) {
    io::mapping none;
    EXPECT_FALSE(none);
    EXPECT_TRUE(none == io::mapping());
    ASSERT_TRUE(write_file(at("m"), "0123456789"));
    for (bool writable : {false, true}) {
        io::map_options end{.writable = writable, .offset = 10};
        auto m = io::map(at("m"), end);
        ASSERT_TRUE(m);
        EXPECT_TRUE(*m);
        EXPECT_EQ(m->size(), 0u);
        EXPECT_TRUE(m->data().empty());
        if (writable) {
            EXPECT_TRUE(m->writable_data().empty());
        }
        EXPECT_TRUE(m->flush());
        EXPECT_FALSE(m->is_closed());
        io::mapping copy = *m;
        EXPECT_TRUE(copy == *m);
        EXPECT_FALSE(copy == io::mapping());
        EXPECT_TRUE(m->close());
        EXPECT_TRUE(copy.is_closed());
        EXPECT_TRUE(m->close());
        EXPECT_EQ(error_of(m->flush()).code(), errc::closed);
        EXPECT_TRUE(m->data().empty());
        io::mapping moved = std::move(copy);
        EXPECT_TRUE(copy == moved);   // a move of the word is a copy
    }
    io::mapping last = io::map(at("m"), io::map_options{.offset = 9, .length = 1});
    EXPECT_EQ(text_of(last.data()), "9");
    EXPECT_EQ(error_of(io::map(at("m"), io::map_options{.offset = 9, .length = 2})).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(io::map(at("m"), io::map_options{.length = UINT64_MAX})).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(io::map(at("m"), io::map_options{.offset = UINT64_MAX})).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(io::map(at("m"), io::map_options{.offset = UINT64_MAX, .length = 1})).code(), std::errc::invalid_argument);
    EXPECT_EQ(text_of(value_of(io::map(at("m"), io::map_options{.offset = 3, .length = 0})).data()), "3456789");
    ASSERT_TRUE(mkdir(at("dir")));
    EXPECT_FALSE(io::map(at("dir")));                                     // a directory has no bytes to map
    EXPECT_FALSE(io::map(at("dir"), io::map_options{.writable = true}));
    EXPECT_EQ(value_of(stat(at("m"))).size, 10u);
}

// Shared memory at its bounds: a name too long, one taken, one missing, a
// size the system refuses (the name not left behind), an object of 0 bytes
// made elsewhere
TEST_F(IoFsBounds_Tests, SharedMemoryAtItsBounds) {
    io::shared_memory none;
    EXPECT_FALSE(none);
    std::string base = "sgcl-b-" + std::to_string(::getpid());
    string name(base.c_str());
    (void)io::shared_memory::remove(name);
    EXPECT_TRUE(error_of(io::shared_memory::open(name)).is_not_found());
    EXPECT_TRUE(error_of(io::shared_memory::remove(name)).is_not_found());
    EXPECT_FALSE(io::shared_memory::create(string(std::string(300, 'n').c_str()), 64));
    EXPECT_FALSE(io::shared_memory::create(name, SIZE_MAX));
    EXPECT_TRUE(error_of(io::shared_memory::open(name)).is_not_found());   // the failed create left no name
    auto made = io::shared_memory::create(name, 1);
    ASSERT_TRUE(made);
    EXPECT_GE(made->size(), 1u);
    EXPECT_TRUE(error_of(io::shared_memory::create(name, 1)).is_exists());
    io::shared_memory moved = std::move(*made);
    EXPECT_TRUE(*made == moved);   // a move of the word is a copy
    EXPECT_TRUE(moved.close());
    ASSERT_TRUE(io::shared_memory::remove(name));

    std::string zero_name = "/" + base + "-z";
    int fd = ::shm_open(zero_name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
    ASSERT_GE(fd, 0);
    ::close(fd);
    auto zero = io::shared_memory::open(string(zero_name.c_str() + 1));
    ASSERT_TRUE(zero) << zero.error().message();
    EXPECT_EQ(zero->size(), 0u);
    EXPECT_TRUE(zero->data().empty());
    EXPECT_TRUE(zero->close());
    EXPECT_TRUE(zero->is_closed());
    EXPECT_TRUE(zero->close());
    ASSERT_TRUE(io::shared_memory::remove(string(zero_name.c_str() + 1)));
}

// Variables: an empty name and one with '=' refused; an empty value is a
// value, which env() takes for unset; a value no T is invalid_argument
TEST(IoOsBounds_Tests, VariablesAtTheirBounds) {
    EXPECT_FALSE(io::getenv(""));
    EXPECT_EQ(error_of(io::setenv("", "x")).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(io::setenv("A=B", "x")).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(io::unsetenv("")).code(), std::errc::invalid_argument);
    EXPECT_TRUE(io::unsetenv("SGCL_BOUNDS_NEVER_SET"));
    ASSERT_TRUE(io::setenv("SGCL_BOUNDS_EMPTY", ""));
    auto v = io::getenv("SGCL_BOUNDS_EMPTY");
    ASSERT_TRUE(v);
    EXPECT_EQ(*v, "");
    EXPECT_EQ(io::env("SGCL_BOUNDS_EMPTY", 7), 7);
    EXPECT_EQ(io::env("SGCL_BOUNDS_EMPTY", string("fallback")), "fallback");
    ASSERT_TRUE(io::setenv("SGCL_BOUNDS_EMPTY", "2147483648"));
    EXPECT_THROW((void)io::env("SGCL_BOUNDS_EMPTY", int(0)), std::invalid_argument);
    EXPECT_EQ(io::env("SGCL_BOUNDS_EMPTY", int64_t(0)), int64_t(2147483648));
    ASSERT_TRUE(io::setenv("SGCL_BOUNDS_EMPTY", "-1"));
    EXPECT_THROW((void)io::env("SGCL_BOUNDS_EMPTY", unsigned(0)), std::invalid_argument);
    ASSERT_TRUE(io::unsetenv("SGCL_BOUNDS_EMPTY"));
    EXPECT_FALSE(io::getenv("SGCL_BOUNDS_EMPTY"));
}

// expand_env at its bounds: nothing, a lone '$', an unclosed brace, an
// empty name, two variables side by side
TEST(IoOsBounds_Tests, ExpandEnvAtItsBounds) {
    ASSERT_TRUE(io::setenv("SGCL_BOUNDS_A", "1"));
    ASSERT_TRUE(io::setenv("SGCL_BOUNDS_B", "2"));
    EXPECT_EQ(io::expand_env(""), "");
    EXPECT_EQ(io::expand_env("$"), "$");
    EXPECT_EQ(io::expand_env("a$"), "a$");
    EXPECT_EQ(io::expand_env("$-"), "$-");
    EXPECT_EQ(io::expand_env("${SGCL_BOUNDS_A"), "${SGCL_BOUNDS_A");
    EXPECT_EQ(io::expand_env("${}"), "");
    EXPECT_EQ(io::expand_env("$SGCL_BOUNDS_A$SGCL_BOUNDS_B"), "12");
    EXPECT_EQ(io::expand_env("${SGCL_BOUNDS_A}${SGCL_BOUNDS_B}"), "12");
    EXPECT_EQ(io::expand_env("$SGCL_BOUNDS_UNSET_NAME."), ".");
    (void)io::unsetenv("SGCL_BOUNDS_A");
    (void)io::unsetenv("SGCL_BOUNDS_B");
}

// The lexical path functions at their bounds: empty, the root, dots
TEST(IoOsBounds_Tests, PathsAtTheirBounds) {
    EXPECT_EQ(path::clean(""), ".");
    EXPECT_EQ(path::clean("/"), "/");
    EXPECT_EQ(path::clean("//"), "/");
    EXPECT_EQ(path::clean("/.."), "/");
    EXPECT_EQ(path::clean("../.."), "../..");
    EXPECT_EQ(path::clean("a/../.."), "..");
    EXPECT_EQ(path::clean("./"), ".");
    EXPECT_EQ(path::base(""), "");
    EXPECT_EQ(path::base("/"), "/");
    EXPECT_EQ(path::base("///"), "/");
    EXPECT_EQ(path::base("a/"), "a");
    EXPECT_EQ(path::dir(""), ".");
    EXPECT_EQ(path::dir("/"), "/");
    EXPECT_EQ(path::dir("a"), ".");
    EXPECT_EQ(path::ext(""), "");
    EXPECT_EQ(path::ext("a.b/"), "");
    EXPECT_EQ(path::ext("."), ".");
    EXPECT_EQ(path::stem(""), "");
    auto [d, f] = path::split("");
    EXPECT_EQ(d, "");
    EXPECT_EQ(f, "");
    EXPECT_EQ(path::join(vector<string>()), "");
    EXPECT_EQ(path::join("", ""), "");
    EXPECT_EQ(path::join("", "a"), "a");
    EXPECT_TRUE(path::split_list("").empty());
    EXPECT_TRUE(path::split_list(":::").empty());
    EXPECT_FALSE(path::is_abs(""));
    EXPECT_FALSE(path::is_local(""));
    EXPECT_EQ(error_of(path::under("d", "")).code(), errc::insecure_path);
    EXPECT_EQ(value_of(path::rel("", "")), ".");
    EXPECT_EQ(value_of(path::rel("/", "/a")), "a");
    EXPECT_EQ(value_of(path::abs("")), value_of(working_dir()));
    EXPECT_TRUE(value_of(path::match("", "")));
    EXPECT_FALSE(value_of(path::match("", "a")));
    EXPECT_TRUE(value_of(path::match("*", "")));
    EXPECT_FALSE(value_of(path::match("*", "a/b")));
}

// A pattern malformed after a separator: the error names the whole
// pattern, as one malformed in its first element does
TEST(IoOsBounds_Tests, AMalformedPatternIsNamedWhole) {
    auto first = path::match("[a", "a");
    ASSERT_FALSE(first);
    EXPECT_EQ(first.error().path(), "[a");
    auto later = path::match("x/[a/b]", "x/a");
    ASSERT_FALSE(later);
    EXPECT_EQ(later.error().code(), errc::invalid_pattern);
    EXPECT_EQ(later.error().path(), "x/[a/b]");
}

// An empty pattern names no file: glob gives nothing (it gave ".", the
// directory the empty name was joined to)
TEST(IoOsBounds_Tests, AnEmptyGlobNamesNothing) {
    EXPECT_TRUE(value_of(path::glob("")).empty());
    EXPECT_FALSE(value_of(path::glob(".")).empty());
}

// The directories the environment names, at its bounds: an empty HOME is
// unset (the password database answers), an empty TMPDIR is /tmp, one with
// a trailing separator is cleaned; chdir to nothing or to a file refused
TEST(IoOsBounds_Tests, DirectoriesFromTheEnvironment) {
    auto home = io::getenv("HOME");
    auto tmp = io::getenv("TMPDIR");
    ASSERT_TRUE(io::setenv("HOME", ""));
    auto fallback = home_dir();
    ASSERT_TRUE(fallback);
    EXPECT_FALSE(fallback->empty());
    ASSERT_TRUE(io::setenv("TMPDIR", ""));
    EXPECT_EQ(temp_dir(), "/tmp");
    ASSERT_TRUE(io::setenv("TMPDIR", "/private/tmp//"));
    EXPECT_EQ(temp_dir(), "/private/tmp");
    if (home) {
        (void)io::setenv("HOME", *home);
    }
    if (tmp) {
        (void)io::setenv("TMPDIR", *tmp);
    } else {
        (void)io::unsetenv("TMPDIR");
    }
    auto before = value_of(working_dir());
    EXPECT_TRUE(error_of(io::chdir("")).is_not_found());
    EXPECT_EQ(error_of(io::chdir(value_of(executable()))).code(), std::errc::not_a_directory);
    EXPECT_EQ(value_of(working_dir()), before);
}

// An error at its bounds: the default one, a count past 32 bits (kept as
// the largest), no operation and no path
TEST(IoOsBounds_Tests, AnErrorAtItsBounds) {
    io::error none;
    EXPECT_EQ(none.code().value(), 0);
    EXPECT_EQ(none.op(), "");
    EXPECT_EQ(none.path(), "");
    EXPECT_EQ(none.count(), 0u);
    EXPECT_FALSE(none.is_eof() || none.is_closed() || none.is_not_found());
    io::error big(errc::unexpected_eof, "read", {}, SIZE_MAX);
    EXPECT_EQ(big.count(), size_t(UINT32_MAX));
    io::error under(errc::unexpected_eof, "read", {}, size_t(UINT32_MAX) - 1);
    EXPECT_EQ(under.count(), size_t(UINT32_MAX) - 1);
    EXPECT_EQ(io::error(errc::closed, "").message(), "stream closed");
    EXPECT_TRUE(io::error(errc::closed, "x") == io::error(errc::closed, "y"));
}

// print and println at their bounds: an empty text, one at and one past the
// room on the stack (256 bytes), a stream that refuses the write (not
// reported, as the page says)
TEST(IoOsBounds_Tests, PrintAtItsBounds) {
    buffer b;
    print(b, "");
    EXPECT_EQ(b.text(), "");
    println(b, "");
    EXPECT_EQ(b.text(), "\n");
    for (size_t n : {size_t(255), size_t(256), size_t(257), size_t(300)}) {
        b.clear();
        std::string text(n, 'p');
        println(b, "{}", std::string_view(text));
        EXPECT_EQ(b.size(), n + 1);
        EXPECT_EQ(b.text().view().substr(0, n), text);
    }
    auto closed = pipe();
    ASSERT_TRUE(closed);
    ASSERT_TRUE(closed->read.close());
    EXPECT_NO_THROW(println(closed->write, "nobody hears"));
    ASSERT_TRUE(closed->write.close());
    EXPECT_NO_THROW(println(closed->write, "closed"));
    EXPECT_EQ(value_of(io::stdout.write(slice<const byte>())), 0u);
}

// A directory entry whose file went after the listing: info() is the error
TEST_F(IoFsBounds_Tests, AnEntryThatWent) {
    ASSERT_TRUE(write_file(at("gone"), "x"));
    auto entries = value_of(read_dir(string(_dir)));
    ASSERT_EQ(entries.size(), 1u);
    ASSERT_TRUE(io::remove(at("gone")));
    EXPECT_TRUE(error_of(entries[0].info()).is_not_found());
    directory_entry empty;
    EXPECT_FALSE(empty.info());
    EXPECT_FALSE(empty.is_directory());
}
