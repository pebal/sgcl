# sgcl::io::buffered_reader, sgcl::io::buffered_writer

```cpp
#include "sgcl/io/buffered.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    class buffered_reader final : public mixin::reader<buffered_reader>;   // a block in front of a reader: lines, prefixes; a handle of one word
    class buffered_writer final : public mixin::writer<buffered_writer>;   // a block in front of a writer: flush; a handle of one word
}
```

Buffering over any reader or writer, `bufio`. The stream is read in blocks of `config::io_buffer_size` (8 KB) into a managed [`array<byte, N>`](../core/array.md) held by a `tracked_ptr` — one object without a header, eight to a page — and the buffered reader hands out lines and prefixes as [slices](../core/slice.md) of that block: `slice<const char>`, the characters in the block and the block held, nothing allocated per line (what `Scanner.Bytes()` costs in Go), the block read once per 8 KB whatever the lines. A slice kept past the next read sees the block refilled; a line kept is copied first, `sgcl::string(line)`, one managed allocation, `Scanner.Text()`. A `buffered_writer` fills its block and writes it when full; `flush()` writes what is there. Its block hands no slice out, so it is unmanaged memory the writer owns while it is written from its thread; the first async operation moves it into a managed block, once, since a task's write may run on the blocking pool and the slice it is given must hold the block.

## Rules

- Both are handles of one word over any stream, held as an `io::reader` / `io::writer` ([stream](stream.md#reader-writer)): `io::buffered_reader lines(io::open(p));`, `io::buffered_writer out(io::stdout);` — the constructor makes the reader's state (its block, its position) at once. The rules of [stream](stream.md#rules).
- A line is a `slice<const char>` into the reader's block, valid as text until the next read: the block is reused, so a line kept across reads is copied first (`sgcl::string(line)`; the string is a copy, the slice is not the whole of a string object). It has the text interface (`contains`, `starts_with`, `find`, `trim`, `substr`, `==` with a literal), so a line is inspected where it lies. `peek` returns a `slice<const byte>` of the block the same way.
- A copy is the same reader or writer: one block, one position, one kept error — what one copy reads the other does not see again; passed by value into a task, the copy keeps it alive for as long as the task runs. A handle is a tracked word: on a stack, in a task, in a managed object; in a global or a std container, a [`rooted`](../core/rooted.md) of it (`rooted<io::buffered_reader> in(std::in_place, f);`, then `in->read_line()`), never in a managed object or a task's frame, since a root is never part of a cycle. A default-constructed one holds none (`!r`).
- `buffered_writer`'s destructor does not flush: it runs on the collector's thread and could report nothing (as `bufio.Writer`: what is not flushed is lost). A buffered writer is flushed or closed when done.
- A line has no bound unless `set_max_line` gives one: a reader over a socket sets it.

## Members

### buffered_reader

```cpp
buffered_reader() noexcept;                                               // none: !r
explicit buffered_reader(const io::reader& r);
expected<size_t, error> read(const slice<byte>& out) const;                // from the block; a read larger than the block goes to r directly
async::task<expected<size_t, error>> async_read(const slice<byte>& out) const;
expected<optional<slice<const char>>, error> read_line() const;         // without "\n" or "\r\n"; the last line without one is a line; nullopt at the end
expected<optional<slice<const char>>, error> read_until(char delimiter) const;   // with the delimiter, as Go's ReadString (the last token of a stream without it); a slice of the block, valid until the next read
async::task<expected<optional<slice<const char>>, error>> async_read_line() const;
async::task<expected<optional<slice<const char>>, error>> async_read_until(char delimiter) const;
expected<slice<const byte>, error> peek(size_t n) const;                     // the next n (fewer at the end, at most the block) without consuming
expected<optional<byte>, error> read_byte() const;
expected<size_t, error> discard(size_t n) const;                                  // skips: the bytes skipped
size_t buffered() const noexcept;                                        // readable without touching r
void set_max_line(size_t n) const noexcept;  size_t max_line() const noexcept;   // 0: no bound; a longer line is errc::line_too_long
generator<slice<const char>> lines() const;                    // for (auto line : r.lines()); ends at the end or on an error
async::generator<slice<const char>> async_lines() const;       // while (auto line = co_await g.next())
const optional<error>& last_error() const noexcept;                      // the error lines() ended on, if any (Scanner.Err())
io::reader underlying() const noexcept;
expected<void, error> close() const;  async::task<expected<void, error>> async_close() const;   // what is buffered dropped, then r's close when r has one
explicit operator bool() const noexcept;  friend bool operator==(const buffered_reader&, const buffered_reader&) noexcept;   // the same reader
```

A line longer than the block is assembled in a vector the reader owns first, and the slice holds that vector's buffer; nothing is lost and nothing is bounded unless asked. `lines()` is a [generator](../core/generator.md) over `read_line`: the range-for of Go's `Scanner`, with the error read after the loop.

```cpp
io::buffered_reader log(io::open("access.log"));
size_t errors = 0;
for (auto line : log.lines()) {
    if (line.contains(" 500 ")) ++errors;
}
if (log.last_error()) eprintln(log.last_error()->message());
```

In a task, the same without holding a thread:

```cpp
async::task<size_t> count(io::reader src) {
    io::buffered_reader in(src);
    in.set_max_line(64 * 1024);                     // a stream that is not trusted
    size_t n = 0;
    auto lines = in.async_lines();
    while (auto line = co_await lines.next()) {      // the lines to the end or the first error
        ++n;
    }
    co_return n;                                    // in.last_error(): the error, if one ended it
}
```

### buffered_writer

```cpp
buffered_writer() noexcept;                                               // none: !w
explicit buffered_writer(const io::writer& w);
expected<size_t, error> write(const slice<const byte>& data) const;        // into the block; written to w when full; a write larger than the block goes to w directly, after the block
async::task<expected<size_t, error>> async_write(const slice<const byte>& data) const;
expected<void, error> flush() const;  async::task<expected<void, error>> async_flush() const;
expected<void, error> close() const;  async::task<expected<void, error>> async_close() const;   // flush, then w's close when w has one
bool is_closed() const noexcept;
size_t buffered() const noexcept;  size_t available() const noexcept;
io::writer underlying() const noexcept;
const optional<error>& last_error() const noexcept;                         // the first error given, kept
explicit operator bool() const noexcept;  friend bool operator==(const buffered_writer&, const buffered_writer&) noexcept;   // the same writer
```

Every error the writer gives is kept as its first (a failure of `w`, a write after close): every write and flush after it gives that error at once and writes nothing, and `close()` gives it (every later `close()` too) after closing `w` all the same, so a buffered writer is written freely and checked once, at the close; `last_error()` holds the error. After the first error, its own included, the writer refuses everything further and `close()` returns that error; whoever wants to react earlier checks the result of a single `write` or `flush`, or `last_error()`.

```cpp
io::buffered_writer out(io::create("out.csv"));
for (auto& row : rows) {
    out.write(row.name);
    out.write(byte(','));
    out.write(to_string(row.count));
    out.write(byte('\n'));
}
if (auto closed = out.close(); !closed) eprintln(closed.error().message());   // the block written, the file closed
```

## Example

```cpp
#include "sgcl/sgcl.h"

using namespace sgcl;

int main(int argc, char** argv) {
    io::reader source = io::stdin;                                  // any stream: the standard input, or the file named
    if (argc > 1) {
        auto opened = io::open(argv[1]);
        if (!opened) {
            eprintln(opened.error().message());
            return 1;
        }
        source = opened;
    }
    io::buffered_reader in(source);
    io::buffered_writer out(io::stdout);
    size_t n = 0;
    for (auto line : in.lines()) {                    // numbered lines, as cat -n
        out.write(to_string(++n));
        out.write("  ");
        out.write(line);                              // the line from the block: no string made
        out.write(byte('\n'));
    }
    out.flush();
    return in.last_error() ? 1 : 0;
}
```

## See also

- [stream](stream.md): the interfaces and the mixins; [file](file.md): what is usually underneath
- `tests/io/buffered.cpp`: lines with and without terminators, a line as a slice holding the block, a line longer than the block, the bound, `read_until`/`peek`/`discard`, the writer's block, the async forms.
