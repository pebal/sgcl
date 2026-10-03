[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](../buffered_reader.md)

# sgcl::io::buffered_reader::lines, async_lines

```cpp
generator<slice<const char>> lines() const noexcept;                 // (1)
async::generator<slice<const char>> async_lines() const noexcept;    // (2)
```

Returns the lines of the stream, from the position on, as a generator over [read_line](read_line.md): each line a
slice of the block, without its end, valid as text until the next one. It is the range-for of Go's `bufio.Scanner`,
with the error read after the loop: the lines end at the end of the stream or on the first error, which
[last_error](last_error.md) holds afterwards, as `Scanner.Err()` does.

1. A [generator](../../core/generator.md) for the calling thread: `for (auto line : in.lines())`.
2. An [async::generator](../../async/task.md) for a task, which may wait between its lines:
   `while (auto line = co_await lines.next())`.

The generator holds the reader, and the reader's position moves as it is iterated; the error of an earlier run is
cleared when it starts.

## Parameters

None.

## Return value

The generator of the lines.

## Complexity

Constant to make; the iteration as [read_line](read_line.md), linear in the length of the stream.

## Exceptions

None. What a read of the stream underneath throws comes out of the iteration: `begin()` and `++` (1), the
`co_await` of `next()` (2).

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_reader log(io::buffer("GET / 200\nGET /a 500\nGET /b 500\nGET / 200\n"));
    size_t errors = 0;
    for (auto line : log.lines()) {
        if (line.contains(" 500")) {
            ++errors;
        }
    }
    if (log.last_error()) {
        eprintln("{}", log.last_error()->message());
    }
    println("{} errors", errors);
}
```

Output:

```text
2 errors
```

In a task, the same without holding a thread, over a stream that is not trusted:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<size_t> count(io::reader source) {
    io::buffered_reader in(source);
    in.set_max_line(64 * 1024);  // a stream that is not trusted
    size_t n = 0;
    auto lines = in.async_lines();
    while (auto line = co_await lines.next()) {  // the lines to the end or the first error
        ++n;
    }
    co_return n;  // in.last_error(): the error, if one ended it
}

int main() {
    println("{} lines", async::run(count(io::buffer("one\ntwo\nthree\n"))));
}
```

Output:

```text
3 lines
```

## See also

- [read_line](read_line.md): one line
- [io::read_lines](../read_lines.md): the lines of a file, in one call
- [last_error](last_error.md): the error the lines ended on
- [generator](../../core/generator.md): the range
- [sgcl::io::buffered_reader](../buffered_reader.md)
