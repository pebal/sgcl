[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](../buffered_reader.md)

# sgcl::io::buffered_reader::buffered_reader

```cpp
buffered_reader() noexcept = default;                      // (1)
explicit buffered_reader(const io::reader& r) noexcept;    // (2)
```

Makes a buffered reader handle.

1. An empty handle, which holds no reader: `!r`, and an operation on it is a contract violation.
2. A reader over the stream `r`: its state is made at once, a block of `config::io_buffer_size` (8 KB) on the managed
   heap and the position at its start. Nothing is read until the first operation. `r` is an
   [io::reader](../reader.md), which any stream converts to — a [file](../file.md), a [buffer](../buffer.md), a
   connection, `io::stdin`, a `tracked_ptr` to a reader of the program's — so `io::buffered_reader in(f);` is
   written with the stream itself. The constructor is explicit: a stream is not taken for a buffered reader where
   one is expected.

A handle of the library given as `r` (a `file`, a `buffer`) is held by its object, so the handle it was made of may
go first.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the stream to read through the block |

## Complexity

Constant: (2) makes the state and its block.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_reader none;
    println("{}", bool(none));

    io::buffer text("first\nsecond\n");
    io::buffered_reader in(text);
    println("{} {}", bool(in), **in.read_line());
    println("{}", text.size());  // the block took all the buffer held
}
```

Output:

```text
false
true first
0
```

## See also

- [operator bool](operator_bool.md): checks whether the handle holds a reader
- [io::reader](../reader.md): what any stream converts to
- [sgcl::io::buffered_reader](../buffered_reader.md)
