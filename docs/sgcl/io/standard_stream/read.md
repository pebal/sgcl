[sgcl](../../README.md) › [io](../README.md) › [standard_stream](../standard_stream.md)

# sgcl::io::standard_stream::read, async_read

```cpp
/*(1)*/ expected<size_t, error> read(const slice<byte>& buffer);
/*(2)*/ async::task<expected<size_t, error>> async_read(const slice<byte>& buffer) noexcept;
```

Reads what is available into `buffer`, at most its size, through the [file](../file.md) over the descriptor: the
[read](../file/read.md) of that file.

1. Waits on the calling thread until some bytes come, or the end.
2. The same as a task: a terminal, a redirected file or a pipe from the shell is read on the
   [blocking pool](../../async/spawn_blocking.md), a non-blocking descriptor on the [reactor](../../async/readable.md).

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go |

## Return value

The number of bytes read, 0 at the end of the stream (a closed pipe, the end of a redirected file, Ctrl-D on a
terminal), or the [error](../error.md) of the read; the operation is `read` and the path the stream's name.

## Complexity

One read of the descriptor, linear in the bytes read.

## Exceptions

- (1) `std::system_error` when the descriptor is non-blocking and the thread of the reactor, or of the timers, which
  its first use starts, cannot be made.
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> block(4096);
    size_t total = 0;
    for (;;) {
        auto n = io::stdin.read(block);
        if (!n || *n == 0) {
            break;
        }
        total += *n;
    }
    println("{} bytes", total);
}
```

Output:

```text
0 bytes
```

## See also

- [mixin::reader](../mixin/reader.md): `read_all_text`, `read_full` and the rest over this read
- [buffered_reader](../buffered_reader.md): lines of the standard input
- [sgcl::io::standard_stream](../standard_stream.md)
