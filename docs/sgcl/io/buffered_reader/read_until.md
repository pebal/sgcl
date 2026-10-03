[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](../buffered_reader.md)

# sgcl::io::buffered_reader::read_until, async_read_until

```cpp
/*(1)*/ expected<optional<slice<const char>>, error> read_until(char delimiter) const;
/*(2)*/ async::task<expected<optional<slice<const char>>, error>>
        async_read_until(char delimiter) const noexcept;
```

Reads up to and including the next `delimiter` and returns the token as a slice of the reader's block. The token
keeps its delimiter, as Go's `bufio.Reader.ReadString` keeps it, so that a stream `"a,b,"` is told from `"a,b"`; the
last token of a stream that does not end in a delimiter is a token too, without one. Like a line of
[read_line](read_line.md), the token is valid as text until the next read, the slice holding the block, and one
longer than the block is assembled in the reader's vector.

1. Reads on the calling thread.
2. The same for a task: the stream's `async_read`, or its `read` on the blocking pool for a stream that has only
   that. The reader is held by the handle the task was made from: the caller keeps it alive until the task is done.

## Parameters

| Parameter | Description |
|---|---|
| `delimiter` | the byte that ends a token |

## Return value

The token; an empty `optional` at the end of the stream; or the error:

- `errc::line_too_long` when a bound is set ([set_max_line](set_max_line.md)) and the token, its delimiter not
  counted, passes it: the token is skipped with its delimiter, and the next call reads the one after it;
- the error of the stream's read, as it gave it.

## Complexity

Linear in the length of the token: a `memchr` over the block, a read of the stream per block.

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
    io::buffered_reader in(io::buffer("a,b,,c"));
    for (;;) {
        auto token = in.read_until(',');
        if (!token || !*token) {
            break;
        }
        println("[{}]", **token);
    }

    io::buffered_reader rest(io::buffer("key=value"));
    println("[{}]", **rest.async_read_until('=').wait());
}
```

Output:

```text
[a,]
[b,]
[,]
[c]
[key=]
```

## See also

- [read_line](read_line.md): a line, its end left out
- [set_max_line](set_max_line.md): the bound of a token
- [sgcl::io::buffered_reader](../buffered_reader.md)
