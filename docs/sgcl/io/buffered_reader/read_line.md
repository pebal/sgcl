[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](README.md)

# sgcl::io::buffered_reader::read_line, async_read_line

```cpp
expected<optional<slice<const char>>, error> read_line() const;                                // (1)
async::task<expected<optional<slice<const char>>, error>> async_read_line() const noexcept;    // (2)
```

Reads the next line and returns it as a slice of the reader's block, without its `"\n"` and without a `"\r"` before
it, so a file written on Windows reads as one written on POSIX. The last line of a stream that does not end in
`"\n"` is a line too; a stream that ends in `"\n"` has no empty line after it. It is Go's
`bufio.Reader.ReadLine`, and `bufio.Scanner` with `ScanLines`, one line per call.

The line is valid as text until the next read: the block is reused, and a line kept across reads is copied first,
`string(line)`. The slice holds the block, so it is never dangling. A line longer than the block (8 KB) is assembled
in a vector the reader owns, and the slice is of that vector's buffer, held the same way. The line is bounded only
by [set_max_line](set_max_line.md).

1. Reads on the calling thread.
2. The same for a task: the stream's `async_read`, or its `read` on the blocking pool for a stream that has only
   that. The reader is held by the handle the task was made from: the caller keeps it alive until the task is done.

## Parameters

None.

## Return value

The line; an empty `optional` at the end of the stream; or the error:

- `errc::line_too_long` when a bound is set and the line passes it ([set_max_line](set_max_line.md)): the line is
  skipped, and the next call reads the one after it;
- the error of the stream's read, as it gave it.

## Complexity

Linear in the length of the line: a `memchr` over the block, a read of the stream per block; a copy into the
reader's vector for a line longer than the block.

## Exceptions

- (1) What the read of the stream underneath throws: a [file](../file/read.md)'s `std::system_error` when the
  reactor's thread cannot be started.
- (2) None. What the read throws is the task's: its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_reader in(io::buffer("GET / HTTP/1.1\r\nHost: example.com\r\n\r\nbody"));
    for (;;) {
        auto line = in.read_line();
        if (!line || !*line || (*line)->empty()) {
            break;  // an error, the end, or the blank line after the header
        }
        println("[{}]", **line);
    }
    auto rest = in.async_read_line().wait();
    println("[{}]", **rest);  // the last line, with no end of its own
    println("{}", bool(*in.read_line()));
}
```

Output:

```text
[GET / HTTP/1.1]
[Host: example.com]
[body]
false
```

## See also

- [lines](lines.md): the lines as a range
- [read_until](read_until.md): a token up to any delimiter, the delimiter kept
- [set_max_line](set_max_line.md): a bound for a stream that is not trusted
- [sgcl::io::buffered_reader](README.md)
