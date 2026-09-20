# sgcl::io::buffered_reader, sgcl::io::buffered_writer

```cpp
#include "sgcl/io/buffered.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    class buffered_reader final : public reader;                  // a block in front of a reader: lines, prefixes
    class buffered_writer final : public writer, public closer;   // a block in front of a writer: flush
}
```

Buffering over any reader or writer, `bufio`. The stream is read in blocks of `config::IoBufferSize` (8 KB) into a managed [`array<std::byte, N>`](../containers/array.md) held by a `tracked_ptr` — one object without a header, eight to a page — and the buffered reader hands out lines and prefixes as [slices](../core/slice.md) of that block: `slice<const char>`, the characters in the block and the block held, nothing allocated per line (what `Scanner.Bytes()` costs in Go), the block read once per 8 KB whatever the lines. A slice kept past the next read sees the block refilled; a line kept is copied first, `sgcl::string(line)`, one managed allocation, `Scanner.Text()`. A `buffered_writer` fills its block and writes it when full; `flush()` writes what is there.

## Rules

- Both are managed objects over a `tracked_ptr<reader>` / `tracked_ptr<writer>`: `sgcl::make_tracked<buffered_reader>(io::open(p).value())`. The rules of [stream](stream.md#rules).
- A line is a `slice<const char>` into the reader's block, valid as text until the next read: the block is reused, so a line kept across reads is copied first (`sgcl::string(line)`; the string is a copy, the slice is not the whole of a string object). It has the text interface (`contains`, `starts_with`, `find`, `trim`, `substr`, `==` with a literal), so a line is inspected where it lies. `peek` returns a `slice<const std::byte>` of the block the same way.
- `buffered_writer`'s destructor does not flush: it runs on the collector's thread and could report nothing (as `bufio.Writer`: what is not flushed is lost). A buffered writer is flushed or closed when done.
- A line has no bound unless `set_max_line` gives one: a reader over a socket sets it.

## Members

### buffered_reader

```cpp
explicit buffered_reader(tracked_ptr<reader> r);
result<size_t> read(slice<std::byte> out) override;                      // from the block; a read larger than the block goes to r directly
task<result<size_t>> async_read(slice<std::byte> out) override;
result<optional<slice<const char>>> read_line();               // without "\n" or "\r\n"; the last line without one is a line; nullopt at the end
result<optional<slice<const char>>> read_until(char delimiter);   // without the delimiter; a slice of the block, valid until the next read
task<result<optional<slice<const char>>>> async_read_line();
task<result<optional<slice<const char>>>> async_read_until(char delimiter);
result<slice<const std::byte>> peek(size_t n);                           // the next n (fewer at the end, at most the block) without consuming
result<optional<std::byte>> read_byte();
result<size_t> discard(size_t n);                                        // skips: the bytes skipped
size_t buffered() const noexcept;                                        // readable without touching r
void set_max_line(size_t n) noexcept;  size_t max_line() const noexcept;   // 0: no bound; a longer line is errc::line_too_long
generator<slice<const char>> lines();                          // for (auto line : r.lines()); ends at the end or on an error
const optional<error>& last_error() const noexcept;                      // the error lines() ended on, if any (Scanner.Err())
tracked_ptr<reader> underlying() const noexcept;
```

A line longer than the block is assembled in a vector the reader owns first, and the slice holds that vector's buffer; nothing is lost and nothing is bounded unless asked. `lines()` is a [generator](../async/coroutine.md#generator) over `read_line`: the range-for of Go's `Scanner`, with the error read after the loop.

```cpp
auto f = io::open("access.log");
sgcl::tracked_ptr r = sgcl::make_tracked<io::buffered_reader>(*f);
size_t errors = 0;
for (auto line : r->lines()) {
    if (line.contains(" 500 ")) ++errors;
}
if (r->last_error()) std::cerr << r->last_error()->message() << '\n';
```

In a task, the same without holding a thread:

```cpp
sgcl::task<size_t> count(sgcl::tracked_ptr<io::reader> src) {
    sgcl::tracked_ptr r = sgcl::make_tracked<io::buffered_reader>(std::move(src));
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
explicit buffered_writer(tracked_ptr<writer> w);
result<size_t> write(slice<const std::byte> data) override;              // into the block; written to w when full; a write larger than the block goes to w directly, after the block
task<result<size_t>> async_write(slice<const std::byte> data) override;
result<void> flush();  task<result<void>> async_flush();
result<void> close() override;                                           // flush, then w's close when w is a closer
bool is_closed() const noexcept override;
size_t buffered() const noexcept;  size_t available() const noexcept;
tracked_ptr<writer> underlying() const noexcept;
```

```cpp
auto f = io::create("out.csv");
sgcl::tracked_ptr w = sgcl::make_tracked<io::buffered_writer>(*f);
for (auto& row : rows) {
    w->write_text(row.name);
    w->write_byte(std::byte(','));
    w->write_text(sgcl::to_string(row.count));
    w->write_byte(std::byte('\n'));
}
if (auto r = w->close(); !r) std::cerr << r.error().message() << '\n';   // the block written, the file closed
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

namespace io = sgcl::io;

int main(int argc, char** argv) {
    auto in = argc > 1 ? io::open(argv[1]) : io::result<sgcl::tracked_ptr<io::file>>(io::stdin());
    if (!in) { std::cerr << in.error().message() << '\n'; return 1; }
    sgcl::tracked_ptr r = sgcl::make_tracked<io::buffered_reader>(*in);
    sgcl::tracked_ptr w = sgcl::make_tracked<io::buffered_writer>(io::stdout());
    size_t n = 0;
    for (auto line : r->lines()) {                    // numbered lines, as cat -n
        w->write_text(sgcl::to_string(++n));
        w->write_text("  ");
        w->write_text(line);                          // the line from the block: no string made
        w->write_byte(std::byte('\n'));
    }
    w->flush();
    return r->last_error() ? 1 : 0;
}
```

## See also

- [stream](stream.md): the interfaces and the mixins; [file](file.md): what is usually underneath
- `tests/io/buffered.cpp`: lines with and without terminators, a line as a slice holding the block, a line longer than the block, the bound, `read_until`/`peek`/`discard`, the writer's block, the async forms.
