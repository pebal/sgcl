[sgcl](../../README.md) › [io](../README.md) › [transform_reader](README.md)

# sgcl::io::transform_reader\<F\>::transform_reader

```cpp
transform_reader(const io::reader& r, F f) noexcept(std::is_nothrow_move_constructible_v<F>);
```

Constructs a reader of `r` that gives what it reads to `f` before it hands it on. The source is held as an
[io::reader](../reader/README.md): any stream converts to one, a handle by its object, a stream of the program's by
reference. `f` is moved in. `F` is deduced from the arguments (the deduction guide), so a lambda is written in the
call.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the source |
| `f` | the function called with the bytes of each read |

## Complexity

Constant.

## Exceptions

What the move constructor of `F` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    size_t lines = 0;
    io::transform_reader counted(io::buffer("one\ntwo\nthree\n"), [&](slice<byte> bytes) {
        for (byte b : bytes) {
            lines += b == byte('\n');
        }
    });
    auto text = counted.read_all_text();
    println("{} bytes, {} lines", text->size(), lines);
}
```

Output:

```text
14 bytes, 3 lines
```

## See also

- [read, async_read](read.md)
- [sgcl::io::transform_reader\<F\>](README.md)
