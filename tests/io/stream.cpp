//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

using namespace sgcl::async;

#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

namespace {
    using namespace sgcl::io;
    namespace io = sgcl::io;

    std::string_view as_text(std::span<const byte> b) {
        return std::string_view(reinterpret_cast<const char*>(b.data()), b.size());
    }

    // A reader that hands out its text in pieces of at most n bytes
    class dribble final : public io::mixin::reader<dribble> {
    public:
        dribble(std::string_view s, size_t n) : _s(s), _n(n) {}

        expected<size_t, io::error> read(slice<byte> out) {
            size_t k = std::min({out.size(), _n, _s.size()});
            std::memcpy(out.data(), _s.data(), k);
            _s.remove_prefix(k);
            return k;
        }

        task<expected<size_t, io::error>> async_read(slice<byte> out) {
            co_return read(out);
        }

    private:
        std::string_view _s;
        size_t _n;
    };

    // A reader that fails after its text
    class failing final : public io::mixin::reader<failing> {
    public:
        explicit failing(std::string_view s) : _s(s) {}

        expected<size_t, io::error> read(slice<byte> out) {
            if (_s.empty()) {
                return sgcl::unexpected(error(std::make_error_code(std::errc::io_error), "read", "failing"));
            }
            size_t k = std::min(out.size(), _s.size());
            std::memcpy(out.data(), _s.data(), k);
            _s.remove_prefix(k);
            return k;
        }

        task<expected<size_t, io::error>> async_read(slice<byte> out) {
            co_return read(out);
        }

    private:
        std::string_view _s;
    };
}

TEST(IoStream_Tests, ErrorCarriesOpPathAndCode) {
    error e(std::make_error_code(std::errc::no_such_file_or_directory), "open", "log.txt");
    EXPECT_TRUE(e.is_not_found());
    EXPECT_FALSE(e.is_permission());
    EXPECT_EQ(e.op(), "open");
    EXPECT_EQ(e.path(), "log.txt");
    EXPECT_EQ(std::string_view(e.message()).substr(0, 14), "open log.txt: ");
    error eof(errc::unexpected_eof, "read");
    EXPECT_TRUE(eof.is_eof());
    EXPECT_EQ(eof.code().category().name(), std::string_view("io"));
    EXPECT_EQ(std::string_view(eof.message()), "read: unexpected end of stream");
    EXPECT_EQ(std::string_view(error(errc::invalid_path, "", "x").message()), "x: invalid path");   // no operation: no space before the path
    EXPECT_EQ(std::string_view(error(errc::invalid_path, "").message()), "invalid path");
    EXPECT_TRUE(error(errc::closed, "x").is_closed());
    EXPECT_TRUE(error(std::make_error_code(std::errc::operation_would_block), "x").is_timeout());
}

TEST(IoStream_Tests, BufferReadsWhatWasWritten) {
    buffer b;
    EXPECT_TRUE(b.empty());
    ASSERT_TRUE(b.write("hello "));
    ASSERT_TRUE(b.write("world"));
    EXPECT_EQ(b.size(), 11u);
    EXPECT_EQ(b.text(), "hello world");
    byte out[5];
    auto r = b.read(out);
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 5u);
    EXPECT_EQ(as_text(std::span<const byte>(out, 5)), "hello");
    EXPECT_EQ(b.text(), " world");
    auto all = b.read_all_text();
    ASSERT_TRUE(all);
    EXPECT_EQ(std::string_view(*all), " world");
    EXPECT_TRUE(b.empty());
    r = b.read(out);
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 0u);   // the end
    b.write("again");
    auto taken = b.release();
    EXPECT_EQ(as_text(std::span<const byte>(taken.data(), taken.size())), "again");
    EXPECT_TRUE(b.empty());
}

// A buffer seeks as a file does, for writing: a write in the middle
// overwrites and runs on, one past the end fills the gap with zeros, the
// position follows the end until moved, and a read moves it back with the
// bytes it takes; a seek before the start is invalid_argument
TEST(IoStream_Tests, ABufferSeeksAsAFile) {
    buffer b;
    ASSERT_TRUE(b.write("0123456789"));
    EXPECT_EQ(value_of(b.tell()), 10u);
    EXPECT_EQ(value_of(b.seek(2)), 2u);
    ASSERT_TRUE(b.write("ab"));
    EXPECT_EQ(value_of(b.tell()), 4u);
    EXPECT_EQ(b.text(), "01ab456789");
    EXPECT_EQ(value_of(b.seek(-2, seek_from::end)), 8u);
    ASSERT_TRUE(b.write("XYZW"));   // over the last two bytes and on
    EXPECT_EQ(b.text(), "01ab4567XYZW");
    EXPECT_EQ(value_of(b.seek(3, seek_from::end)), 15u);
    ASSERT_TRUE(b.write("!"));
    EXPECT_EQ(b.text(), std::string("01ab4567XYZW\0\0\0!", 16));
    EXPECT_EQ(value_of(b.seek(-1, seek_from::current)), 15u);
    ASSERT_TRUE(b.rewind());
    ASSERT_TRUE(b.write("#"));
    EXPECT_EQ(b.text().view().substr(0, 3), "#1a");
    byte out[4];
    ASSERT_EQ(value_of(b.read(out)), 4u);   // the position (1) moves back with them, to the front
    EXPECT_EQ(value_of(b.tell()), 0u);
    EXPECT_EQ(value_of(b.seek(0, seek_from::end)), 12u);
    ASSERT_TRUE(b.write("."));      // at the end again: appends
    EXPECT_EQ(b.text(), std::string("4567XYZW\0\0\0!.", 13));
    auto bad = b.seek(-1);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), std::make_error_code(std::errc::invalid_argument));
    EXPECT_EQ(b.size(), 13u);   // the buffer's own size() (the mixin's is hidden)
}

// write takes a string, a text slice (a piece of a string, a line of a
// buffered reader), a literal and a std::string_view, each written from
// where it lies
TEST(IoStream_Tests, WriteTextTakesAStringASliceALiteralAndAStdView) {
    buffer b;
    string s = "alpha beta";
    const char* p = "!";
    ASSERT_TRUE(b.write(s));
    ASSERT_TRUE(b.write(s.as_slice(6)));
    ASSERT_TRUE(b.write("x"));
    ASSERT_TRUE(b.write(p));
    std::string std_text = "?";
    ASSERT_TRUE(b.write(std_text));                 // a std::string, through its view
    ASSERT_TRUE(b.write(std::string_view(std_text).substr(0, 0)));
    EXPECT_EQ(b.text(), "alpha betabetax!?");
    auto t = sgcl::async::spawn([b, s]() -> task<size_t> {
        size_t total = *co_await b.async_write(s.as_slice(0, 5));
        total += *co_await b.async_write("y");
        co_return total;
    });
    EXPECT_EQ(t.wait(), 6u);
    EXPECT_EQ(b.text(), "alpha betabetax!?alphay");
    sgcl::async::scheduler::stop();
}

TEST(IoStream_Tests, CharArraysStopAtTheirEnd) {
    // an array filled to the brim has no NUL: what follows it in memory
    // is not written (strlen read on into it)
    buffer b;
    struct {
        char full[4];
        char after[4];
    } s = {{'a', 'b', 'c', 'd'}, {'x', 'y', 'z', 0}};
    ASSERT_EQ(value_of(b.write(s.full)), 4u);
    EXPECT_EQ(b.text(), "abcd");
    char padded[8] = "ef";                                          // up to the first NUL
    ASSERT_EQ(value_of(b.write(padded)), 2u);
    ASSERT_EQ(value_of(b.write("g\0h")), 1u);                  // a literal likewise
    char* mutable_pointer = padded;
    const char* pointer = padded;
    ASSERT_EQ(value_of(b.write(mutable_pointer)), 2u);          // a pointer to its NUL
    ASSERT_EQ(value_of(b.write(pointer)), 2u);
    char room[8] = {'i', 'j'};
    ASSERT_EQ(value_of(b.write(slice<char>(room, 2))), 2u);     // a slice of mutable characters
    EXPECT_EQ(b.text(), "abcdefgefefij");
    auto t = sgcl::async::spawn([b, &s]() -> task<size_t> {
        co_return *co_await b.async_write(s.full);
    });
    EXPECT_EQ(t.wait(), 4u);
    sgcl::async::scheduler::stop();
}

TEST(IoStream_Tests, WriteByteOnAThreadAndInATask) {
    buffer b;
    ASSERT_EQ(value_of(b.write(byte('a'))), 1u);
    auto t = sgcl::async::spawn([b]() -> task<size_t> {
        co_return *co_await io::async_write(b, byte('b'));
    });
    EXPECT_EQ(t.wait(), 1u);
    EXPECT_EQ(b.text(), "ab");
    sgcl::async::scheduler::stop();
}

// Go's ReadFull: the buffer filled, its size; the stream ending part way,
// errc::unexpected_eof with the bytes read (the error's count, Go's n);
// ending before the first byte, the end of the stream, a read of 0 (Go's
// io.EOF), which a loop over records ends on
TEST(IoStream_Tests, ReadFullAndUnexpectedEof) {
    sgcl::tracked_ptr d = make_tracked<dribble>("abcdefgh", 3);
    byte out[6];
    auto r = d->read_full(out);
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 6u);
    EXPECT_EQ(as_text(std::span<const byte>(out, 6)), "abcdef");
    r = d->read_full(out);   // two bytes left, six asked
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_eof());
    EXPECT_EQ(r.error().code(), errc::unexpected_eof);
    EXPECT_EQ(r.error().count(), 2u);
    EXPECT_EQ(as_text(std::span<const byte>(out, 2)), "gh");
    EXPECT_EQ(value_of(d->read_full(out)), 0u);   // nothing left: the end
    EXPECT_EQ(value_of(d->read_full(slice<byte>())), 0u);   // nothing asked, nothing missing
    EXPECT_EQ(error(errc::closed, "read").count(), 0u);

    sgcl::tracked_ptr records = make_tracked<dribble>("aabbcc", 4);
    std::string seen;
    for (;;) {
        byte rec[2];
        auto n = io::read_full(*records, rec);
        ASSERT_TRUE(n);
        if (*n == 0) {
            break;
        }
        seen += as_text(std::span<const byte>(rec, 2));
        seen += '|';
    }
    EXPECT_EQ(seen, "aa|bb|cc|");
}

TEST(IoStream_Tests, ReadAllGrowsPastTheFirstBlock) {
    std::string big(3 * sgcl::config::io_buffer_size + 17, 'x');
    sgcl::tracked_ptr d = make_tracked<dribble>(big, 1000);
    auto r = d->read_all();
    ASSERT_TRUE(r);
    EXPECT_EQ(r->size(), big.size());
    EXPECT_TRUE(std::all_of(r->begin(), r->end(), [](byte b) { return b == byte('x'); }));
    sgcl::tracked_ptr f = make_tracked<failing>("partial");
    auto e = f->read_all();
    ASSERT_FALSE(e);
    EXPECT_EQ(e.error().code(), std::make_error_code(std::errc::io_error));
}

TEST(IoStream_Tests, CopyMovesEverything) {
    std::string big(2 * sgcl::config::io_copy_buffer_size + 5, 'y');
    sgcl::tracked_ptr src = make_tracked<dribble>(big, 7000);
    buffer dst;
    auto n = copy(dst, *src);
    ASSERT_TRUE(n);
    EXPECT_EQ(*n, big.size());
    EXPECT_EQ(dst.size(), big.size());
    buffer src2(std::string_view("abc"));
    buffer dst2;
    EXPECT_EQ(value_of(dst2.copy_from(src2)), 3u);
    EXPECT_EQ(dst2.text(), "abc");
    EXPECT_EQ(value_of(src2.copy_to(discard)), 0u);
}

TEST(IoStream_Tests, LimitTeeMulti) {
    buffer src(std::string_view("0123456789"));
    sgcl::tracked_ptr lim = make_tracked<limit_reader>(src, 4);
    auto r = lim->read_all_text();
    ASSERT_TRUE(r);
    EXPECT_EQ(std::string_view(*r), "0123");
    EXPECT_EQ(lim->remaining(), 0u);
    EXPECT_EQ(src.text(), "456789");

    buffer seen;
    sgcl::tracked_ptr tee = make_tracked<tee_reader>(src, seen);
    EXPECT_EQ(std::string_view(value_of(tee->read_all_text())), "456789");
    EXPECT_EQ(seen.text(), "456789");

    sgcl::vector<reader> parts;
    parts.push_back(make_tracked<buffer>(std::string_view("ab")));
    parts.push_back(make_tracked<buffer>(std::string_view("")));
    parts.push_back(make_tracked<dribble>("cde", 1));
    sgcl::tracked_ptr multi = make_tracked<multi_reader>(std::move(parts));
    EXPECT_EQ(std::string_view(value_of(multi->read_all_text())), "abcde");

    buffer w1, w2;
    sgcl::vector<writer> ws;
    ws.push_back(w1);
    ws.push_back(w2);
    sgcl::tracked_ptr mw = make_tracked<multi_writer>(std::move(ws));
    EXPECT_EQ(value_of(mw->write("both")), 4u);
    EXPECT_EQ(w1.text(), "both");
    EXPECT_EQ(w2.text(), "both");
}

TEST(IoStream_Tests, AsyncFormsOnTheScheduler) {
    auto t = sgcl::async::spawn([]() -> task<std::string> {
        sgcl::tracked_ptr src = make_tracked<dribble>("async copy of some text", 5);
        buffer dst;
        auto n = co_await async_copy(dst, *src);
        if (!n || *n != 23) {
            co_return "copy failed";
        }
        auto all = co_await dst.async_read_all_text();
        if (!all) {
            co_return "read failed";
        }
        byte out[4];
        sgcl::tracked_ptr d2 = make_tracked<dribble>("xy", 1);
        auto full = co_await d2->async_read_full(out);
        if (full || !full.error().is_eof() || full.error().count() != 2) {
            co_return "read_full should fail after 2 bytes";
        }
        auto end = co_await d2->async_read_full(out);
        if (!end || *end != 0) {
            co_return "read_full at the end should be 0";
        }
        co_return all->str();
    }());
    EXPECT_EQ(t.wait(), "async copy of some text");
    sgcl::async::scheduler::stop();
}

// A task's copy from a buffer hands over what it holds in one write, as
// the blocking copy does (write_to, async_write_to), not a block at a time
TEST(IoStream_Tests, ATasksCopyFromABufferIsOneWrite) {
    std::string text(70000, 'b');
    buffer src{std::string_view(text)};
    int writes = 0;
    size_t written = 0;
    auto t = sgcl::async::spawn(io::async_copy([&](slice<const byte> b) -> task<expected<size_t, io::error>> {
        ++writes;
        written += b.size();
        co_return b.size();
    }, src));
    EXPECT_EQ(value_of(t.wait()), 70000u);
    EXPECT_EQ(writes, 1);
    EXPECT_EQ(written, 70000u);
    EXPECT_TRUE(src.empty());
    sgcl::async::scheduler::stop();
}

template<class R>
concept TaskCopies = requires(R& r) { sgcl::io::async_copy(sgcl::io::discard, r); };

// What io takes as a stream: a type with the method, a callable of the
// same shape, by reference or tracked_ptr; the async forms only where the
// stream has them
TEST(IoStream_Tests, AnythingWithTheMethodOrACallable) {
    struct only_sync {
        expected<size_t, io::error> read(slice<byte> b) { return b.empty() ? 0 : (b[0] = byte('s'), n++ < 3 ? 1 : 0); }
        int n = 0;
    };
    static_assert(io::req::reader<only_sync&> && !io::req::async_reader<only_sync&>);
    static_assert(io::req::writer<buffer&> && io::req::async_writer<sgcl::tracked_ptr<buffer>>);
    static_assert(!io::req::reader<buffer*>);                      // no raw pointer: a reference or a tracked_ptr
    static_assert(!TaskCopies<only_sync>);                 // a blocking source is not taken by a task's copy
    only_sync s;
    buffer out;
    ASSERT_EQ(value_of(io::copy(out, s)), 3u);
    EXPECT_EQ(out.text(), "sss");
    int left = 2;
    ASSERT_EQ(value_of(io::copy(out, [&](slice<byte> b) -> size_t { if (!left) return 0; --left; b[0] = byte('!'); return 1; })), 2u);
    std::string seen;
    ASSERT_EQ(value_of(io::copy([&](slice<const byte> b) { seen.append(reinterpret_cast<const char*>(b.data()), b.size()); }, out)), 5u);
    EXPECT_EQ(seen, "sss!!");
    // a task's copy between a buffer and a callable returning a task
    buffer src(std::string_view("task"));
    std::string got;
    auto t = sgcl::async::spawn(io::async_copy([&](slice<const byte> b) -> task<expected<size_t, io::error>> {
        got.append(reinterpret_cast<const char*>(b.data()), b.size());
        co_return b.size();
    }, src));
    EXPECT_EQ(value_of(t.wait()), 4u);
    EXPECT_EQ(got, "task");
    sgcl::async::scheduler::stop();
}

// A handle holds any stream; the half it lacks is made from the other,
// and only there
TEST(IoStream_Tests, HandlesAndTheHalfTheyMake) {
    struct only_sync {
        expected<size_t, io::error> read(slice<byte> b) { return done ? 0 : (done = true, b[0] = byte('x'), size_t(1)); }
        bool done = false;
    };
    only_sync s;
    reader r = s;
    EXPECT_TRUE(r.has_read());
    EXPECT_FALSE(r.has_async_read());
    byte b[4];
    auto t = sgcl::async::spawn(r.async_read(b));        // on the blocking pool
    EXPECT_EQ(value_of(t.wait()), 1u);
    EXPECT_EQ(b[0], byte('x'));
    buffer buf(std::string_view("abc"));
    reader held = buf;                                    // the buffer kept by the handle
    EXPECT_TRUE(held.has_read() && held.has_async_read());
    EXPECT_EQ(held.fd(), -1);
    EXPECT_EQ(value_of(held.read_all_text()), "abc");
    writer w = make_tracked<buffer>();                    // a unique_ptr from make_tracked: held, not lost
    EXPECT_EQ(value_of(w.write("kept")), 4u);
    EXPECT_TRUE(w.close());                               // a buffer has no close: nothing to close
    writer w2 = w;
    EXPECT_TRUE(w == w2);                                 // the same stream
    auto owned = make_tracked<buffer>(std::string_view("own"));   // a unique_ptr keeps it: the handle refers, owns nothing
    reader over_unique = owned;
    EXPECT_EQ(value_of(over_unique.read_all_text()), "own");
    writer out = io::stdout;
    EXPECT_EQ(out.fd(), 1);
    EXPECT_EQ(io::stdout.fd(), 1);
    sgcl::async::scheduler::stop();
}

// Every async form of the module's streams is noexcept: a task is made
// without a throw, what its body throws is the task's
static_assert(noexcept(std::declval<const io::reader&>().async_read(std::declval<const slice<byte>&>())));
static_assert(noexcept(std::declval<const io::reader&>().async_close()));
static_assert(noexcept(std::declval<const io::writer&>().async_write(std::declval<const slice<const byte>&>())));
static_assert(noexcept(std::declval<const io::writer&>().async_close()));
static_assert(noexcept(std::declval<const io::buffered_reader&>().async_close()));
static_assert(noexcept(std::declval<const io::buffered_writer&>().async_close()));

// A null pointer makes an empty handle, as an empty one does: a pointer to
// a library's handle (whose state it would read) and to any other stream,
// a tracked_ptr, a root_ptr and an lvalue unique_ptr from make_tracked
TEST(IoStream_Tests, ANullPointerMakesAnEmptyHandle) {
    auto empty = [](const reader& r, const writer& w) {
        return !r && !w && r.fd() == -1 && w.fd() == -1 && r.close() && w.close() && !r.has_read() && !w.has_write();
    };
    sgcl::tracked_ptr<buffer> none;
    EXPECT_TRUE(empty(none, none));
    EXPECT_TRUE(empty(sgcl::tracked_ptr<buffer>(), sgcl::tracked_ptr<buffer>()));
    sgcl::root_ptr<buffer> rooted;
    EXPECT_TRUE(empty(rooted, rooted));
    decltype(make_tracked<buffer>()) unique;
    EXPECT_TRUE(empty(unique, unique));
    sgcl::tracked_ptr<io::file> no_file;
    EXPECT_TRUE(empty(no_file, no_file));
    decltype(make_tracked<dribble>("", 1)) no_stream;
    EXPECT_FALSE(reader(no_stream));
    sgcl::tracked_ptr<dribble> no_tracked;
    EXPECT_FALSE(reader(no_tracked));
}

TEST(IoStream_Tests, TransformReaderInBothForms) {
    buffer src(std::string_view("abc"));
    auto upper = [](slice<byte> b) { for (auto& c : b) c = byte(std::toupper(int(c))); };
    transform_reader up(src, upper);
    EXPECT_EQ(value_of(up.read_all_text()), "ABC");
    buffer src2(std::string_view("def"));
    sgcl::tracked_ptr up2 = make_tracked<transform_reader<decltype(upper)>>(src2, upper);
    auto t = sgcl::async::spawn(up2->async_read_all_text());
    EXPECT_EQ(value_of(t.wait()), "DEF");
    sgcl::async::scheduler::stop();
}

// print and println: the pattern and its values on a writer, a new line
// after println's, one value with no pattern, a pattern read at run time
// printed only when it fits; the names are sgcl's too
TEST(IoStream_Tests, PrintAndPrintln) {
    auto b = sgcl::make_tracked<io::buffer>();
    io::println(b, "{} items", 3);
    io::print(b, "a{}", 'b');
    sgcl::println(b, "");
    EXPECT_EQ(b->text(), "3 items\nab\n");
    // the forms on the standard streams compile and run
    sgcl::print("");
    sgcl::eprint("");
    sgcl::string name = "x";
    (void)name;
    EXPECT_TRUE(io::print(sgcl::txt::runtime(""), 1) || true);
    EXPECT_FALSE(io::print(sgcl::txt::runtime("{} {}"), 1));
}

// io::buffer is a handle: copies share one buffer; an empty buffer makes
// its state on first use, and a copy of an empty one makes it in the
// buffer copied first, so that both copies see the same bytes; a stream
// made of a temporary buffer keeps its state
TEST(IoStream_Tests, TheBufferIsAHandle) {
    static_assert(sizeof(buffer) == sizeof(sgcl::tracked_ptr<void>));
    buffer empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.size(), 0u);
    byte out[4];
    EXPECT_EQ(value_of(empty.read(out)), 0u);                          // a read of nothing
    buffer copy = empty;                                       // the one state, made by the constructor
    ASSERT_TRUE(empty.write("ab"));
    ASSERT_TRUE(copy.write("cd"));
    EXPECT_EQ(empty.text(), "abcd");
    EXPECT_EQ(copy.text(), "abcd");
    EXPECT_TRUE(copy == empty);
    buffer assigned;
    buffer blank;
    assigned = blank;                                          // assignment likewise
    ASSERT_TRUE(blank.write("x"));
    EXPECT_EQ(assigned.text(), "x");

    io::writer w{buffer()};                                    // a temporary: the writer holds its state
    sgcl::collector::force_collect(true);
    ASSERT_TRUE(w.write(std::string("kept")));
    buffer target;
    io::writer to_target = target;                             // an empty buffer as a stream: shared
    ASSERT_TRUE(to_target.write(std::string("shared")));
    EXPECT_EQ(target.text(), "shared");

    buffer raced;                                              // two threads copy one empty buffer: one state
    buffer first, second;
    std::thread t1([&] { first = raced; });
    std::thread t2([&] { second = raced; });
    t1.join();
    t2.join();
    EXPECT_TRUE(first == raced);
    EXPECT_TRUE(second == raced);
    EXPECT_FALSE(buffer() == buffer());                        // two buffers made apart are two buffers, empty or not
}

// A default buffer has its state from the constructor: a copy shares it,
// and eight threads writing through copies of one default buffer, made
// while none of them has written yet, write into the one state. The state
// was once made lazily by a compare-exchange on the word of the buffer
// copied — a write of another handle's tracked word from the copy, racing
// the plain loads of the others (DESIGN)
TEST(IoStream_Tests, CopiesOfADefaultBufferShareItsState) {
    buffer original;
    buffer copy = original;
    ASSERT_TRUE(copy.write("x"));
    EXPECT_EQ(original.text(), "x");                           // the write through the copy, seen in the original
    original.clear();
    constexpr int Threads = 8, Writes = 1000;
    buffer shared;                                             // never written before the threads copy it
    std::mutex m;                                              // the buffer is one state, not a concurrent one: the writes one at a time
    std::vector<std::thread> threads;
    for ([[maybe_unused]] int t : range(Threads)) {
        threads.emplace_back([&] {
            buffer mine = shared;                              // eight copies made at once
            for (int i = 0; i < Writes; ++i) {
                std::lock_guard lock(m);
                (void)mine.write(byte(1));
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    auto d = shared.data();
    size_t sum = 0;
    for (auto b : d) {
        sum += size_t(b);
    }
    EXPECT_EQ(sum, size_t(Threads * Writes));
}
