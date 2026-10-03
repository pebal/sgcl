[sgcl](../README.md) › [io](README.md)

# sgcl::io::buffered_reader

```cpp
#include "sgcl/io/buffered.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class buffered_reader;
}
```

`io::buffered_reader` is a reader with a buffer in front of any other: Go's `bufio.Reader` and `bufio.Scanner` in one
class. The stream underneath is read in blocks of `config::io_buffer_size` (8 KB) into a managed `array<byte, N>`
held by a `tracked_ptr` — one object without a header, `config::page_size / config::io_buffer_size` of them (eight)
to a page — and the reader hands out lines, tokens and prefixes as [slices](../core/slice.md) of that block:
`slice<const char>`, the characters in the block and the block held. Nothing is allocated per line, which is what
Go's `Scanner.Bytes()` costs, and the stream is read once per 8 KB whatever the lines; a line kept is copied first,
`string(line)`, one managed allocation, Go's `Scanner.Text()`.

It is a handle of one word, a `tracked_ptr` to the reader's state (its block, its position, its bound, the error
`lines()` ended on), made by the constructor over any stream held as an [io::reader](reader.md):
`io::buffered_reader in(f);` over a [file](file.md), a [buffer](buffer.md), a connection, `io::stdin`. Copies share
the state, as the copies of a `file` share the file. `std` has no counterpart: an `std::istream` buffers inside its
`streambuf` and `std::getline` copies every line into a `std::string`.

## Rules

- A copy is the same reader: one block, one position. What one copy reads the other does not see again; passed by
  value into a task, the copy keeps the reader alive for as long as the task runs. Two readers made over the same
  stream are two blocks and two positions, each reading what the other has not taken.
- A handle is a tracked word: on a stack, in a task, in a managed object; in a global or a std container, a
  [rooted](../core/rooted.md) of it (`rooted<io::buffered_reader> in(std::in_place, f);`, then
  `in->read_line()`), never in a managed object or a task's frame, since a root is never part of a cycle.
- A default-constructed reader holds no state (`!r`); an operation on it is a contract violation, asserted in
  debug builds.
- A line is a `slice<const char>` into the reader's block, valid as text until the next read: the block is reused,
  so a line kept across reads is copied first (`string(line)`). The slice holds the block, so it is never
  dangling, but its characters are those of the moment it was made only until the reader reads again. It has the
  text interface (`contains`, `starts_with`, `find`, `trim`, `substr`, `==` with a literal): a line is inspected
  where it lies. [peek](buffered_reader/peek.md) returns a `slice<const byte>` of the block the same way.
- A line longer than the block is assembled in a vector the reader owns, and the slice is of that vector's buffer,
  held likewise: nothing is lost, and nothing is bounded unless [set_max_line](buffered_reader/set_max_line.md)
  gives a bound. A file is trusted; a reader over a socket sets one.
- One thread or task at a time on one reader.
- The reader closes nothing by itself: [close](buffered_reader/close.md) closes the stream underneath. Its
  destructor runs on the collector's thread, after the sweep that finds it dead.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](buffered_reader/buffered_reader.md) | makes a reader over a stream, or an empty handle |
| `(destructor)` | lets go of the state; the reader is the collector's when no handle holds it |

#### Reading

| Function | Description |
|---|---|
| [read, async_read](buffered_reader/read.md) | reads bytes, from the block first |
| [read_byte](buffered_reader/read_byte.md) | reads one byte |
| [peek](buffered_reader/peek.md) | the next bytes, not consumed |
| [discard](buffered_reader/discard.md) | skips bytes |

#### Lines and tokens

| Function | Description |
|---|---|
| [read_line, async_read_line](buffered_reader/read_line.md) | the next line, without its end |
| [read_until, async_read_until](buffered_reader/read_until.md) | the next token, up to a delimiter and with it |
| [lines, async_lines](buffered_reader/lines.md) | the lines of the stream as a range |
| [last_error](buffered_reader/last_error.md) | the error the lines ended on |
| [set_max_line](buffered_reader/set_max_line.md) | bounds the length of a line |
| [max_line](buffered_reader/max_line.md) | the bound of a line |

#### Observers

| Function | Description |
|---|---|
| [buffered](buffered_reader/buffered.md) | the number of bytes in the block, not yet read |
| [underlying](buffered_reader/underlying.md) | the stream underneath |
| [operator bool](buffered_reader/operator_bool.md) | checks whether the handle holds a reader |

#### Closing

| Function | Description |
|---|---|
| [close, async_close](buffered_reader/close.md) | drops what is buffered and closes the stream underneath |

#### From mixin::reader

The rest of a reader over `read` ([mixin::reader](mixin/reader.md)).

| Function | Description |
|---|---|
| [read_full, async_read_full](mixin/reader/read_full.md) | fills the whole buffer, or says why not |
| [read_all, async_read_all](mixin/reader/read_all.md) | everything to the end of the stream, as bytes |
| [read_all_text, async_read_all_text](mixin/reader/read_all_text.md) | everything to the end of the stream, as a string |
| [copy_to, async_copy_to](mixin/reader/copy_to.md) | the stream to its end, written to a writer |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](buffered_reader/operator_cmp.md) | checks whether two handles are the same reader |

## Complexity

The stream underneath is read once per 8 KB, whatever the lines; a line is found by `memchr` over the block, and
costs no allocation unless it is longer than the block.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_reader in(io::buffer("alpha\nbeta\r\ngamma"));
    io::buffered_writer out(io::stdout);
    size_t n = 0;
    for (auto line : in.lines()) {  // numbered lines, as cat -n
        out.write(to_string(++n));
        out.write("  ");
        out.write(line);  // the line from the block: no string made
        out.write(byte('\n'));
    }
    out.flush();
    return in.last_error() ? 1 : 0;
}
```

Output:

```text
1  alpha
2  beta
3  gamma
```

## See also

- [buffered_writer](buffered_writer.md): the same in front of a writer
- [io::reader](reader.md): what the reader is made over
- [file](file.md): what is usually underneath
- [slice](../core/slice.md): what a line is
- `tests/io/buffered.cpp`: lines with and without terminators, a line as a slice holding the block, a line longer
  than the block, the bound, `read_until`, `peek`, `discard`, the async forms
