# sgcl::io::buffered_reader, sgcl::io::buffered_writer

```cpp
#include "sgcl/io/buffered.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    class buffered_reader final : public mixin::reader<buffered_reader>;   // a block in front of a reader: lines, prefixes
    class buffered_writer final : public mixin::writer<buffered_writer>;   // a block in front of a writer: flush
}
```

Buffering over any reader or writer, `bufio`. The stream is read in blocks of `config::io_buffer_size` (8 KB) into a managed [`array<byte, N>`](../core/array.md) held by a `tracked_ptr` — one object without a header, eight to a page — and the buffered reader hands out lines and prefixes as [slices](../core/slice.md) of that block: `slice<const char>`, the characters in the block and the block held, nothing allocated per line (what `Scanner.Bytes()` costs in Go), the block read once per 8 KB whatever the lines. A slice kept past the next read sees the block refilled; a line kept is copied first, `sgcl::string(line)`, one managed allocation, `Scanner.Text()`. A `buffered_writer` fills its block and writes it when full; `flush()` writes what is there.

## Rules

- Both are managed objects over any stream, held as an `io::reader` / `io::writer` ([stream](stream.md#reader-writer)): `sgcl::make_tracked<buffered_reader>(io::open(p).value())`, `make_tracked<buffered_writer>(io::stdout)`. The rules of [stream](stream.md#rules).
- A line is a `slice<const char>` into the reader's block, valid as text until the next read: the block is reused, so a line kept across reads is copied first (`sgcl::string(line)`; the string is a copy, the slice is not the whole of a string object). It has the text interface (`contains`, `starts_with`, `find`, `trim`, `substr`, `==` with a literal), so a line is inspected where it lies. `peek` returns a `slice<const byte>` of the block the same way.
- A `buffered_reader` is moved, not copied, as a `std::ifstream` is not: a copy would share the block with a position of its own, and each would read over what the other refills. A `buffered_writer` the same: a copy would share the block with a fill of its own, and the bytes not yet flushed would go out twice or be written over. Each is passed on by reference, or held by a [handle](stream.md) (`io::reader`, `io::writer`), which refers to it.
- `buffered_writer`'s destructor does not flush: it runs on the collector's thread and could report nothing (as `bufio.Writer`: what is not flushed is lost). A buffered writer is flushed or closed when done.
- A line has no bound unless `set_max_line` gives one: a reader over a socket sets it.

## Members

### buffered_reader

```cpp
explicit buffered_reader(const io::reader& r);
expected<size_t, error> read(const slice<byte>& out);                      // from the block; a read larger than the block goes to r directly
async::task<expected<size_t, error>> async_read(slice<byte> out);
expected<optional<slice<const char>>, error> read_line();               // without "\n" or "\r\n"; the last line without one is a line; nullopt at the end
expected<optional<slice<const char>>, error> read_until(char delimiter);   // with the delimiter, as Go's ReadString (the last token of a stream without it); a slice of the block, valid until the next read
async::task<expected<optional<slice<const char>>, error>> async_read_line();
async::task<expected<optional<slice<const char>>, error>> async_read_until(char delimiter);
expected<slice<const byte>, error> peek(size_t n);                           // the next n (fewer at the end, at most the block) without consuming
expected<optional<byte>, error> read_byte();
expected<size_t, error> discard(size_t n);                                        // skips: the bytes skipped
size_t buffered() const noexcept;                                        // readable without touching r
void set_max_line(size_t n) noexcept;  size_t max_line() const noexcept;   // 0: no bound; a longer line is errc::line_too_long
generator<slice<const char>> lines();                          // for (auto line : r.lines()); ends at the end or on an error
async::generator<slice<const char>> async_lines();             // while (auto line = co_await g.next())
const optional<error>& last_error() const noexcept;                      // the error lines() ended on, if any (Scanner.Err())
io::reader underlying() const noexcept;
expected<void, error> close();  async::task<expected<void, error>> async_close();   // what is buffered dropped, then r's close when r has one
```

A line longer than the block is assembled in a vector the reader owns first, and the slice holds that vector's buffer; nothing is lost and nothing is bounded unless asked. `lines()` is a [generator](../core/generator.md) over `read_line`: the range-for of Go's `Scanner`, with the error read after the loop.

```cpp
auto f = io::open("access.log");
tracked_ptr r = make_tracked<io::buffered_reader>(*f);
size_t errors = 0;
for (auto line : r->lines()) {
    if (line.contains(" 500 ")) ++errors;
}
if (r->last_error()) std::cerr << r->last_error()->message() << '\n';
```

In a task, the same without holding a thread:

```cpp
async::task<size_t> count(io::reader src) {
    tracked_ptr r = make_tracked<io::buffered_reader>(std::move(src));
    r->set_max_line(64 * 1024);                     // a stream that is not trusted
    size_t n = 0;
    while (auto line = co_await r->async_read_line()) {
        if (!*line) break;                          // the end
        ++n;
    }
    co_return n;
}
```

### buffered_writer

```cpp
explicit buffered_writer(const io::writer& w);
expected<size_t, error> write(const slice<const byte>& data);              // into the block; written to w when full; a write larger than the block goes to w directly, after the block
async::task<expected<size_t, error>> async_write(slice<const byte> data);
expected<void, error> flush();  async::task<expected<void, error>> async_flush();
expected<void, error> close();  async::task<expected<void, error>> async_close();   // flush, then w's close when w has one
bool is_closed() const noexcept;
size_t buffered() const noexcept;  size_t available() const noexcept;
io::writer underlying() const noexcept;
```

```cpp
auto f = io::create("out.csv");
tracked_ptr w = make_tracked<io::buffered_writer>(*f);
for (auto& row : rows) {
    w->write(row.name);
    w->write(byte(','));
    w->write(to_string(row.count));
    w->write(byte('\n'));
}
if (auto r = w->close(); !r) std::cerr << r.error().message() << '\n';   // the block written, the file closed
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

int main(int argc, char** argv) {
    io::reader in = io::stdin;                                  // any stream: the standard input, or the file named
    if (argc > 1) {
        auto f = io::open(argv[1]);
        if (!f) { std::cerr << f.error().message() << '\n'; return 1; }
        in = *f;
    }
    tracked_ptr r = make_tracked<io::buffered_reader>(in);
    tracked_ptr w = make_tracked<io::buffered_writer>(io::stdout);
    size_t n = 0;
    for (auto line : r->lines()) {                    // numbered lines, as cat -n
        w->write(to_string(++n));
        w->write("  ");
        w->write(line);                          // the line from the block: no string made
        w->write(byte('\n'));
    }
    w->flush();
    return r->last_error() ? 1 : 0;
}
```

## See also

- [stream](stream.md): the interfaces and the mixins; [file](file.md): what is usually underneath
- `tests/io/buffered.cpp`: lines with and without terminators, a line as a slice holding the block, a line longer than the block, the bound, `read_until`/`peek`/`discard`, the writer's block, the async forms.
