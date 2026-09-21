# sgcl::io::reader, writer, seeker, closer, stream

```cpp
#include "sgcl/io/stream.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    namespace mixin {                         // sgcl/io/mixin/: the operations over one primitive
        template<class Derived> class reader; //   Derived::read
        template<class Derived> class writer; //   Derived::write
        template<class Derived> class seeker; //   Derived::seek
    }
    class reader : public mixin::reader<reader>;   // virtual read, async_read
    class writer : public mixin::writer<writer>;   // virtual write, async_write
    class seeker : public mixin::seeker<seeker>;   // virtual seek
    class closer;                             // virtual close, is_closed
    class stream : public reader, public writer, public closer;
    enum class seek_from { begin, current, end };

    result<size_t> copy(writer& w, reader& r);
    task<result<size_t>> async_copy(writer& w, reader& r);
    class limit_reader;  class tee_reader;  class multi_reader;  class multi_writer;
    tracked_ptr<writer> discard();
    class buffer;                             // bytes.Buffer: read from the front, written at the back
}
```

A stream is an interface of one primitive — `reader::read(slice<byte>)`, `writer::write(slice<const byte>)`, `seeker::seek(offset, from)`, `closer::close()` — each pure virtual, with its asynchronous twin (`async_read`, `async_write`), and a mixin over the primitive with everything else (`namespace io::mixin`, the headers of `sgcl/io/mixin/`): `mixin::reader<Derived>` gives `read_full`, `read_all`, `read_all_text`, `copy_to` and their `async_` forms to whatever has a `read`, as [`mixin::enumerable`](../core/mixin/enumerable.md) gives the questions to whatever has `begin()` and `end()`. `reader` is `mixin::reader<reader>` with `read` virtual, so every class derived from it — `file`, `buffer`, `buffered_reader`, a socket, a gzip reader — has the whole set, and a `tracked_ptr<reader>` holds any of them: what an `io.Reader` value is in Go, the method table in the object rather than beside the pointer. A class of your own is a reader by deriving from `reader` and defining `read` and `async_read`; nothing else.

`read` returns the bytes read, 0 at the end of the stream (a read of an empty slice returns 0 without touching the stream), and may return fewer than asked; `write` writes the whole slice, as Go's `Write`, and returns its size, fewer only with an error that says how far it got. `write_text` and `read_all_text` are the same bytes as text: `read_all_text` a [`string`](../core/string.md), `write_text` a string, a text [slice](../core/slice.md) (a piece of a string, a line of a [buffered_reader](buffered.md)), a literal or a `std::string_view` (a `std::string`'s characters), written from where it lies, no string made (the `const char*` overload is also what keeps a literal from being ambiguous between the string and the slice). They have names of their own so that a class overriding `write` does not hide them.

## Rules

- A stream is a managed object: `sgcl::make_tracked<buffer>()`, `io::open(p)`; held by a `tracked_ptr`, passed by `tracked_ptr<reader>` or by reference. It lives where a managed object may.
- The destructor runs on the collector's thread after the sweep that finds the stream dead, which may be long after the last use: a stream that holds a descriptor is `close()`d when done, which releases it now and reports the error a deferred close cannot.
- An async operation is a coroutine of the stream: its frame holds the stream by `this`, so the caller keeps its `tracked_ptr` for as long as it awaits — which `co_await r->async_read(b)` in a task does by itself.
- Errors are values: `result<T>`, never an exception ([error](error.md)). The end is not an error.
- One thread or task at a time on one stream; two readers of one `file` use `read_at` with positions of their own.

## Members

### mixin::reader

```cpp
result<size_t> read_full(slice<std::byte> buffer);   // fills the slice: its size; the stream ending first is errc::unexpected_eof, or 0 before the first byte
result<vector<std::byte>> read_all();                    // to the end
result<string> read_all_text();
result<size_t> copy_to(writer& w);                       // to the end, into w: the bytes copied, config::IoCopyBufferSize at a time through a managed array
task<result<size_t>> async_read_full(slice<std::byte> buffer);
task<result<vector<std::byte>>> async_read_all();
task<result<string>> async_read_all_text();
task<result<size_t>> async_copy_to(writer& w);
```

### mixin::writer

```cpp
result<size_t> write_text(const string& text);           // and (const slice<const char>&), (const char*), (std::string_view): a string, a piece of one or a reader's line, a literal, a std::string's characters, each written from where it lies
result<size_t> write_byte(std::byte b);
result<size_t> copy_from(reader& r);                     // r.copy_to(*this)
task<result<size_t>> async_write_text(const string& text);   // the same four
task<result<size_t>> async_copy_from(reader& r);
```

### mixin::seeker

```cpp
result<uint64_t> tell();      // seek(0, current)
result<uint64_t> size();      // the end, the position kept
result<void> rewind();        // seek(0)
```

### reader, writer, seeker, closer, stream

```cpp
class reader
: public mixin::reader<reader> {
    virtual result<size_t> read(slice<std::byte> buffer) = 0;
    virtual task<result<size_t>> async_read(slice<std::byte> buffer) = 0;
};
class writer
: public mixin::writer<writer> {
    virtual result<size_t> write(slice<const std::byte> data) = 0;
    virtual task<result<size_t>> async_write(slice<const std::byte> data) = 0;
};
class seeker
: public mixin::seeker<seeker> {
    virtual result<uint64_t> seek(int64_t offset, seek_from from = seek_from::begin) = 0;
};
class closer {
    virtual result<void> close() = 0;                    // a second close does nothing and succeeds; a read or write after it is errc::closed
    virtual bool is_closed() const noexcept = 0;
};
class stream : public reader, public writer, public closer {};
```

A reader of your own, in memory, with the async form that needs no waiting:

```cpp
class counting_reader final : public io::reader {
public:
    explicit counting_reader(tracked_ptr<io::reader> r) : _r(std::move(r)) {}
    io::result<size_t> read(slice<std::byte> b) override {
        auto n = _r->read(b);
        if (n) bytes += *n;
        return n;
    }
    task<io::result<size_t>> async_read(slice<std::byte> b) override {
        auto n = co_await _r->async_read(b);
        if (n) bytes += *n;
        co_return n;
    }
    size_t bytes = 0;
private:
    tracked_ptr<io::reader> _r;
};
```

### copy, async_copy

```cpp
result<size_t> copy(writer& w, reader& r);               // r.copy_to(w)
task<result<size_t>> async_copy(writer& w, reader& r);
```

### limit_reader, tee_reader, multi_reader, multi_writer, discard

```cpp
limit_reader(tracked_ptr<reader> r, uint64_t n);   uint64_t remaining() const noexcept;   // the first n bytes of r, then the end
tee_reader(tracked_ptr<reader> r, tracked_ptr<writer> w);                                 // what is read is written to w too; w's error is the read's
explicit multi_reader(vector<tracked_ptr<reader>> readers);                               // one after another
explicit multi_writer(vector<tracked_ptr<writer>> writers);                               // to every one; the first error stops it
tracked_ptr<writer> discard();                                                            // drops everything
```

```cpp
tracked_ptr head = make_tracked<io::limit_reader>(*io::open("big.bin"), 1024);   // the first kilobyte
auto bytes = head->read_all();
```

### buffer

```cpp
buffer();
explicit buffer(slice<const std::byte> initial);
explicit buffer(const string& initial);
result<size_t> read(slice<std::byte> out) override;          // consumes from the front
result<size_t> write(slice<const std::byte> in) override;    // appends
slice<const std::byte> data() const noexcept;                    // what remains: a slice of the buffer's storage, its bytes valid until the next write
const string& text() const noexcept;
size_t size() const noexcept;  bool empty() const noexcept;
void clear() noexcept;  void reserve(size_t n);
vector<std::byte> release();                                     // takes the bytes out, the buffer empty
```

A growing block of bytes in memory that is read from the front and written at the back, `bytes.Buffer`: a reader for a parser to consume, a writer for a response to accumulate, the stream of a test.

```cpp
tracked_ptr out = make_tracked<io::buffer>();
out->write_text("GET / HTTP/1.1\r\n");
out->write_text("Host: example.com\r\n\r\n");
auto n = io::copy(*socket, *out);   // the whole request in one write
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// A reader that upper-cases ASCII on the way through
class upper_reader final : public io::reader {
public:
    explicit upper_reader(tracked_ptr<io::reader> r) : _r(std::move(r)) {}
    io::result<size_t> read(slice<std::byte> b) override {
        auto n = _r->read(b);
        if (n) for (auto& c : b.first(*n)) c = std::byte(std::toupper(int(c)));
        return n;
    }
    task<io::result<size_t>> async_read(slice<std::byte> b) override {
        auto n = co_await _r->async_read(b);
        if (n) for (auto& c : b.first(*n)) c = std::byte(std::toupper(int(c)));
        co_return n;
    }
private:
    tracked_ptr<io::reader> _r;
};

int main() {
    tracked_ptr src = make_tracked<io::buffer>("hello, streams\n");
    tracked_ptr up = make_tracked<upper_reader>(src);
    auto n = io::copy(*io::stdout(), *up);            // HELLO, STREAMS
    std::cout << *n << " bytes\n";
}
```

## See also

- [buffered](buffered.md): lines and blocks over any reader; [file](file.md): the stream over a descriptor; [error](error.md): `result`
- [the mixins](../core/mixin/README.md): the same pattern for containers
- `tests/io/stream.cpp`: every behaviour above, checked.
