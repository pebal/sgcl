# sgcl::io::reader, writer, the requirements and the streams

```cpp
#include "sgcl/io/stream.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    namespace req {                           // sgcl/io/req.h: what io takes as a stream
        template<class T> concept reader;         // read(slice<byte>), or a callable of that shape
        template<class T> concept async_reader;   // async_read(slice<byte>) → task, or a callable returning one
        template<class T> concept writer;         // write(slice<const byte>), or a callable
        template<class T> concept async_writer;
        template<class T> concept closer;         // close()
        template<class T> concept async_closer;   // async_close()
        template<class T> concept seeker;         // seek(offset, from)
    }
    namespace mixin {                         // sgcl/io/mixin/: the rest over the primitive, for a class of its own
        template<class Derived> class reader; //   read_full, read_all, read_all_text, copy_to and their async_ forms
        template<class Derived> class writer; //   write of text or a byte, copy_from and their async_ forms
        template<class Derived> class seeker; //   tell, size, rewind
    }
    enum class seek_from { begin, current, end };

    class reader;                             // any reader, as a value (Go's interface value)
    class writer;                             // any writer, as a value

    // sgcl/io/functions.h: the algorithms, for any stream
    expected<size_t, error> copy(req::writer auto&& w, req::reader auto&& r);
    task<expected<size_t, error>> async_copy(req::async_writer auto&& w, req::async_reader auto&& r);
    expected<size_t, error> read_full(req::reader auto&& r, const slice<byte>& b);   // and read_all, read_all_text, write of any data; async_ each

    class limit_reader;  class tee_reader;  class multi_reader;  class multi_writer;
    template<class F> class transform_reader;
    inline discard_writer discard;            // io.Discard
    class buffer;                             // bytes.Buffer: read from the front, written at the back
}
```

A stream is whatever has the primitive: `read(slice<byte>)`, which returns the bytes read, 0 at the end of the stream, and `write(slice<const byte>)`, which writes all of it, as Go's `Write`, or fails with an error that says how far it got. Both do their work now, on the calling thread: io is synchronous, as `read` and `write` are in POSIX, in `std` and in Go. A stream that can wait without holding a thread — a socket, a pipe on the reactor — has `async_read` and `async_write` as well, which return a `task` for `co_await`. No base class and nothing virtual on the stream's side: a class of your own is a reader by having `read`; a lambda that fills a buffer and says how much is a reader too, and one that takes the bytes is a writer. The requirements (`io::req::reader`, `req::async_reader`, `req::writer`, `req::async_writer`, `req::closer`, `req::async_closer`, `req::seeker`) are concepts, checked where the stream is passed: `io::async_copy` of a source that has only `read` does not compile, rather than hold a thread of the pool for every wait.

The functions of io take any of them — a handle of the library (a `file`, a `buffer`, a `buffered_reader`, a `connection`), an object by reference (`io::stdout`, a stream of your own on the stack), a `tracked_ptr` to one on the managed heap, a callable — and the name without a prefix is the blocking form, `async_` the task's: `io::copy(io::stdout, up)` and `co_await io::async_copy(io::stdout, up)`; `read_full`, `read_all`, `read_all_text`, `write`, `write` the same. The classes of the library (`file`, `buffer`, `buffered_reader`, `connection`, the encoders) have the same as methods, from the mixins (`mixin::reader<Derived>`, `mixin::writer<Derived>`, `mixin::seeker<Derived>`), each the algorithm of `functions.h` over the class; a class of your own gets them by deriving from the mixin, with no virtual anywhere. `write` writes a string, a text slice (a line of a [buffered_reader](buffered.md)), a literal or a character array (to its first NUL, never past its end), a C string or a `std::string_view`, each from where it lies.

Where a stream has to be kept — the source of a `buffered_reader`, the output of an encoder, a field of your class — `io::reader` and `io::writer` hold any of them as a value: a `tracked_ptr` of what keeps the stream, a pointer to it and a table of its methods, Go's interface value. A handle of the library is held by its object (the copies of a handle share it); a stream of your own given by reference is referenced (its managed object kept, if it lies in one; one on a stack or a global is the caller's to keep alive), one given by `tracked_ptr` held, a callable or a temporary copied into a managed object of its own. The half a stream lacks is made from the other, and only in the handle: `read` of a stream that has only `async_read` waits for its task on the calling thread, `async_read` of one that has only `read` runs it on the [blocking pool](../async/blocking.md), a thread of which it holds for the whole wait; `has_read()` and `has_async_read()` say which halves are the stream's own; `io::reader(r)` written out is where the choice is made.

## Rules

- A stream of the library is a handle of one word (`io::buffer out;`, `io::file f = io::open(p);`, `io::buffered_reader in(f);`): the handle is copyable, the copies share the object. A handle is a tracked word: on a stack, in a task, in a managed object; in a global or a std container, a [`rooted`](../core/rooted.md) of it (`rooted<io::buffer> log(std::in_place);`, then `log->write(...)`), never in a managed object or a task's frame, since a root is never part of a cycle. The three standard streams (`io::stdin`, `io::stdout`, `io::stderr`, [os](os.md)) are objects of their own. No raw pointer is taken: a reference for an object on a stack or a global, a `tracked_ptr` for one on the managed heap.
- The destructor runs on the collector's thread after the sweep that finds the stream dead, which may be long after the last use: a stream that holds a descriptor is `close()`d when done, which releases it now and reports the error a deferred close cannot.
- An async form is a coroutine over the stream: a stream given by reference is held by that reference while the task runs, so the caller keeps it alive until the task is done — which `co_await io::async_copy(w, r)` in one statement does by itself; a temporary (a lambda written in the call) is moved into the task's frame.
- Errors are values: `expected<T, error>`, never an exception ([error](error.md)). The end is not an error.
- One thread or task at a time on one stream; two readers of one `file` use `read_at` with positions of their own.

## Members

### io::req

```cpp
template<class T> concept reader;         // t.read(slice<byte>) → expected<size_t, error> or size_t; or t(slice<byte>) → the same
template<class T> concept async_reader;   // t.async_read(slice<byte>) → task<expected<size_t, error>>; or t(...) → that task
template<class T> concept writer;         // t.write(slice<const byte>) → expected<size_t, error> or size_t; or t(...) → that, or void
template<class T> concept async_writer;
template<class T> concept closer;         // t.close() → expected<void, error>
template<class T> concept async_closer;   // t.async_close() → task<expected<void, error>>
template<class T> concept seeker;         // t.seek(int64_t, seek_from) → expected<uint64_t, error>
```

T is the argument as passed: a type, a reference to it, or a `tracked_ptr` to it (the concept looks through the pointer). A method wins over a call operator where a type has both; a type with `read` or `write` is never taken as a callable. A plain `size_t` in place of `expected<size_t, error>` is a stream that does not fail.

### copy, read_full, read_all, read_all_text, write

```cpp
expected<size_t, error> copy(req::writer auto&& w, req::reader auto&& r);                 // r to its end into w
task<expected<size_t, error>> async_copy(req::async_writer auto&& w, req::async_reader auto&& r);
expected<size_t, error> read_full(req::reader auto&& r, const slice<byte>& b);             // all of b, or errc::unexpected_eof (the end before the first byte included)
expected<vector<byte>, error> read_all(req::reader auto&& r);
expected<string, error> read_all_text(req::reader auto&& r);
expected<size_t, error> write(req::writer auto&& w, const auto& data);               // bytes (a slice, a vector, an array) or text (a string, a slice, a literal, a char array, a C string, a string_view)
expected<size_t, error> write(req::writer auto&& w, byte b);
// async_read_full, async_read_all, async_read_all_text, async_write: the same, a task
```

`copy` moves the bytes through one block of `config::io_copy_buffer_size` (32 KB), on the stack of the call; `async_copy`'s is managed, since its reads may run on the blocking pool and the slice a read is given holds the block; or in one call when the source has a way of its own (`write_to`, and `async_write_to` for `async_copy`, as Go's `WriterTo`: a `buffer` hands over what it holds). `read_all` gathers the stream in unmanaged memory, grown by doubling, and makes one `vector<byte>` of exactly its size at the end: one managed allocation, the result. `async_read_all` reads into managed blocks instead (8 KB, then 32 KB each for a longer stream), which a read on the pool holds, and copies them once into its result.

### mixin::reader, mixin::writer, mixin::seeker

```cpp
// mixin::reader<Derived>, over Derived::read (and async_read where Derived has it)
expected<size_t, error> read_full(const slice<byte>& b);             task<expected<size_t, error>> async_read_full(const slice<byte>& b);
expected<vector<byte>, error> read_all();                     task<expected<vector<byte>, error>> async_read_all();
expected<string, error> read_all_text();                           task<expected<string, error>> async_read_all_text();
expected<size_t, error> copy_to(req::writer auto&& w);             task<expected<size_t, error>> async_copy_to(req::async_writer auto&& w);

// mixin::writer<Derived>, over Derived::write (and async_write)
expected<size_t, error> write(const auto& text);              task<expected<size_t, error>> async_write(const auto& text);   // beside Derived's write(slice<const byte>)
expected<size_t, error> write(byte b);                   task<expected<size_t, error>> async_write(byte b);
expected<size_t, error> copy_from(req::reader auto&& r);           task<expected<size_t, error>> async_copy_from(req::async_reader auto&& r);

// mixin::seeker<Derived>, over Derived::seek
expected<uint64_t, error> tell();  expected<uint64_t, error> size();  expected<void, error> rewind();
```

The async forms exist where Derived has `async_read` (`async_write`). A class that defines `write` hides the mixin's overloads of the name, as C++ hides a base's name, and brings them back with `using mixin::writer<Derived>::write;` and `using mixin::writer<Derived>::async_write;`, as the classes of the library do; a class of your own that does not derive from the mixin writes text through `io::write(w, text)`.

### reader, writer

```cpp
class reader : public mixin::reader<reader> {
    reader() noexcept;                                    // empty
    reader(R&& r);                                        // any R with req::reader or req::async_reader
    expected<size_t, error> read(const slice<byte>& b) const;          // const: a call through the handle, as through a pointer
    task<expected<size_t, error>> async_read(const slice<byte>& b) const;
    expected<void, error> close() const;                           // the stream's close, as a writer's (Go's ReadCloser); nothing to close: success
    task<expected<void, error>> async_close() const;               // its async_close, or its close on the blocking pool
    bool has_read() const noexcept;  bool has_async_read() const noexcept;  bool has_close() const noexcept;
    int fd() const noexcept;                              // the descriptor under the stream, -1 for none
    explicit operator bool() const noexcept;
    friend bool operator==(const reader&, const reader&) noexcept;   // the same stream
};
class writer : public mixin::writer<writer> {
    writer(W&& w);                                        // any W with req::writer or req::async_writer
    expected<size_t, error> write(const slice<const byte>& d) const;
    task<expected<size_t, error>> async_write(const slice<const byte>& d) const;
    expected<void, error> close() const;                           // the stream's close; nothing to close: success
    task<expected<void, error>> async_close() const;               // its async_close, or its close on the blocking pool
    bool has_write() const noexcept;  bool has_async_write() const noexcept;  bool has_close() const noexcept;
    int fd() const noexcept;  explicit operator bool() const noexcept;
};
```

Three words: the `tracked_ptr` of what keeps the stream, a pointer to it, a pointer to a table of its methods made once per type. Copying copies the words.

A handle of the library — `file`, `buffer`, `buffered_reader`, `buffered_writer`, `net::connection` — is copyable, and the copies share one object; `io::reader`/`io::writer` made of one holds that object itself, so the handle it was made of may go first. A class of your own given by reference is referenced (the managed object it lies in held; one on a stack or a global is the caller's to keep alive), one given by `tracked_ptr` held, a callable or a temporary copied into a managed object of its own.

### limit_reader, tee_reader, multi_reader, multi_writer, transform_reader, discard

```cpp
limit_reader(const io::reader& r, uint64_t n);                   // the first n bytes of r, then the end; remaining()
tee_reader(const io::reader& r, const io::writer& w);                   // what is read from r is written to w as well (io.TeeReader)
multi_reader(vector<io::reader> readers);                 // one after another (io.MultiReader)
multi_writer(vector<io::writer> writers);                 // every one of them; the first error stops it (io.MultiWriter)
transform_reader(const io::reader& r, F f);                      // f(slice<byte>) changes the bytes just read, in both forms
inline discard_writer discard;                            // drops everything (io.Discard): io::copy(io::discard, r)
```

Each has `read` and `async_read` (`write` and `async_write`), over its streams' own; over a stream that has only the blocking half, the async form goes through the handle, and so through the blocking pool.

### buffer

```cpp
buffer() noexcept;  explicit buffer(const slice<const byte>& initial);  explicit buffer(const string& initial);
expected<size_t, error> read(const slice<byte>& out) const;         task<expected<size_t, error>> async_read(const slice<byte>& out) const;
expected<size_t, error> write(const slice<const byte>& in) const;   task<expected<size_t, error>> async_write(const slice<const byte>& in) const;
slice<const byte> data() const noexcept;             // what remains: valid until the next write
string text() const;  size_t size() const noexcept;  bool empty() const noexcept;
void clear() const noexcept;  void reserve(size_t n) const;  vector<byte> release() const;
expected<uint64_t, error> seek(int64_t offset, seek_from from = seek_from::begin) const;   // + tell(), rewind() (mixin::seeker)
friend bool operator==(const buffer&, const buffer&) noexcept;   // the same buffer
```

A growing block of bytes in memory (Go's `bytes.Buffer`), read from the front and written at the back: a parser's input, a response being built, the stream of a test. Its async forms never wait. As the source of `io::copy` it hands over what it holds in one write.

A handle of one word, as `file` is: a copy is the same buffer, and both copies write to and read from the same bytes. `io::buffer out;` makes its (empty) buffer at once, as every constructor does.

It seeks as a file does, for writing: the write position counts from the first byte held (`data()`), follows the end until a seek moves it, and a write there overwrites what is held and runs on past the end; a seek past the end is allowed, and a write there fills the gap with zeros, as `os.File` and `pwrite` do (a `std::stringstream` refuses the seek instead). Reads still consume from the front, and the position moves back with the bytes they take; a seek before the first byte is `std::errc::invalid_argument`. `seek(0, seek_from::end)` puts the position back at the end, where writes append. It is what a writer that must come back to its start needs in memory: a [7z archive](../compress/sevenzip.md) writes its signature header last. The buffer's own `size()` (bytes held) hides the mixin's.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <cctype>
#include <cstring>

using namespace sgcl;

// A reader that upper-cases ASCII on the way through: a class with one
// method, for a task, and no base class (io::transform_reader does the
// same in a line)
class upper_reader {
public:
    explicit upper_reader(const io::reader& r) : _r(r) {}

    async::task<expected<size_t, io::error>> async_read(slice<byte> b) {
        auto n = co_await _r.async_read(b);
        if (n) for (auto& c : b.first(n)) c = byte(std::toupper(int(c)));
        co_return n;
    }

private:
    io::reader _r;
};

int main() {
    io::buffer greeting("hello, streams\n");
    upper_reader up(greeting);                                    // an io::reader holding the buffer (the table under reader, writer)
    auto n = io::async_copy(io::stdout, up).wait();                    // a task, waited for by this thread: HELLO, STREAMS
    if (n) {
        println("{} bytes", *n);
    }

    // a lambda is a reader too, here a blocking one: it fills the buffer and
    // says how much, 0 at the end
    const char text[] = "and a lambda\n";
    bool done = false;
    io::copy(io::stdout, [&](slice<byte> b) -> size_t {
        if (done) return 0;
        done = true;
        std::memcpy(b.data(), text, sizeof(text) - 1);
        return sizeof(text) - 1;
    });
}
```

Output:

```text
HELLO, STREAMS
15 bytes
and a lambda
```

## See also

- [buffered](buffered.md): lines and blocks over any reader; [file](file.md): the stream over a descriptor; [error](error.md): `error`
- [the mixins](../core/mixin/README.md): the same pattern for containers
- `tests/io/stream.cpp`: every behaviour above, checked.
