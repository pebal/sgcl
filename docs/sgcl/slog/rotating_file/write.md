[sgcl](../../README.md) › [slog](../README.md) › [rotating_file](README.md)

# sgcl::slog::rotating_file::write

```cpp
expected<size_t, io::error> write(const slice<const byte>& data) const;
```

Appends `data` to the file by one `write`, from the calling thread, with no lock. A write that would take the file
past `max_size` (a file not empty), or that comes after the cron's next time, rotates the file first. The primitive
of io's writer: the writes of text and of a byte of [mixin::writer](../../io/mixin/writer/README.md) come here, and so
do a logger's lines.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes, a line of a log as a rule |

## Return value

The bytes written, or the `io::error` of the write or of the rotation; `errc::closed` after [close](close.md).

## Complexity

One system call; a rotation a rename and an open more.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::rotating_file out = slog::rotating_file::open("app.log", {.max_size = 10, .keep = 0});
    out.write("12345678\n");
    out.write("abc\n");  // would pass 10 bytes: rotated first
    println("{}", out.size());
}
```

Output:

```text
4
```

## See also

- [rotate](rotate.md)
- [sgcl::slog::rotating_file](README.md)
