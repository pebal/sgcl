[sgcl](../../README.md) › [compress](../README.md) › [tar](../tar.md) › [writer](README.md)

# sgcl::compress::tar::writer::write, async_write

```cpp
expected<size_t, io::error> write(const slice<const byte>& data);                         // (1)
async::task<expected<size_t, io::error>> async_write(slice<const byte> data) noexcept;    // (2)
```

Writes the current entry's data: all of `data`, or nothing when it goes past the entry's size, which is
`errc::invalid_argument`, kept as the writer's first error. The entry's data is written in as many writes as the
program likes, `size` bytes in all.

1. Blocks the calling thread for the write of `out`.
2. Returns a task that does the same and gives the worker back while `out` writes. The bytes are read when the task
   runs: `data` lives until the task is done.

The text forms (a string, a literal, a `std::string_view`) and one byte come from
[mixin::writer](../../io/mixin/writer/README.md), and so does `copy_from`, which writes a
whole reader's bytes into the entry.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the entry's data |

## Return value

The number of bytes written, `data.size()`, or an error: data past the entry's size or before a header (an
`io::error` of `errc::invalid_argument` naming the entry), a write after the close, a failure of `out`, or the error
kept from before.

## Complexity

Linear in the size of `data`.

## Exceptions

- (1) What the `write` of `out` throws; the errors of the stream and of the archive are returned.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::tar::writer w(archive);
    (void)w.write_header({.name = "a.txt", .size = 3});
    println("{}", *w.write("ab"));
    println("{}", w.write("cd").error().message());  // 4 bytes in all: past the size
    println("{}", w.last_error()->message());
}
```

Output:

```text
2
write tar entry a.txt: invalid argument
tar: entry a.txt: a write past its size
```

## See also

- [write_header](write_header.md)
- [sgcl::compress::tar::writer](README.md)
