[sgcl](../../README.md) › [io](../README.md) › [buffered_writer](../buffered_writer.md)

# sgcl::io::buffered_writer::buffered_writer

```cpp
buffered_writer() noexcept = default;                      // (1)
explicit buffered_writer(const io::writer& w) noexcept;    // (2)
```

Makes a buffered writer handle.

1. An empty handle, which holds no writer: `!w`, and an operation on it is a contract violation.
2. A writer over the stream `w`: its state is made at once, with a block of `config::io_buffer_size` (8 KB) of
   unmanaged memory it owns, empty. `w` is an [io::writer](../writer.md), which any stream converts to — a
   [file](../file.md), a [buffer](../buffer.md), a connection, `io::stdout`, a `tracked_ptr` to a writer of the
   program's — so `io::buffered_writer out(io::stdout);` is written with the stream itself. The constructor is
   explicit: a stream is not taken for a buffered writer where one is expected.

## Parameters

| Parameter | Description |
|---|---|
| `w` | the stream to write through the block |

## Complexity

Constant: (2) makes the state and its block.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_writer none;
    println("{}", bool(none));

    io::buffered_writer out(io::stdout);
    out.write("through the block\n");
    out.flush();
    println("{}", out.available());
}
```

Output:

```text
false
through the block
8192
```

## See also

- [operator bool](operator_bool.md): checks whether the handle holds a writer
- [io::writer](../writer.md): what any stream converts to
- [sgcl::io::buffered_writer](../buffered_writer.md)
