[sgcl](../../README.md) › [io](../README.md) › [standard_stream](../standard_stream.md)

# sgcl::io::standard_stream::write, async_write

```cpp
expected<size_t, error> write(const slice<const byte>& data);                                // (1)
async::task<expected<size_t, error>> async_write(const slice<const byte>& data) noexcept;    // (2)
```

Writes all of `data` through the [file](../file.md) over the descriptor, the [write](../file/write.md) of that file.
The text and the single byte of [mixin::writer](../mixin/writer/write.md) are written by the same names, which the
class brings in beside these: `io::stdout.write("text\n")`.

1. Waits on the calling thread until every byte is written.
2. The same as a task, on the [blocking pool](../../async/spawn_blocking.md) or the [reactor](../../async/readable.md) as
   the descriptor is blocking or not.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to write |

## Return value

The number of bytes written, all of `data`, or the [error](../error.md) of the write, which says how far it got;
the operation is `write` and the path the stream's name. A write to a pipe whose reader is gone raises `SIGPIPE`,
which ends the process as a shell's `prog | head` expects, as Go's standard output does; a program that ignores the
signal gets `EPIPE`. A [file](../file.md) io opens or makes takes the signal off.

## Complexity

Linear in the size of `data`.

## Exceptions

- (1) `std::system_error` when the descriptor is non-blocking and the thread of the reactor, or of the timers, which
  its first use starts, cannot be made.
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string greeting = "hello\n";
    auto n = io::stdout.write(greeting);
    (void)io::stdout.write(byte('!'));
    (void)io::stdout.write("\n");
    println("{} bytes", n.value_or(0));
}
```

Output:

```text
hello
!
6 bytes
```

## See also

- [print](../print.md), [println](../println.md): formatted text on the standard output
- [mixin::writer](../mixin/writer.md): the text and the byte, `copy_from`
- [sgcl::io::standard_stream](../standard_stream.md)
