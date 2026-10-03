[sgcl](../../README.md) › [compress](../README.md) › [lzw](../lzw/README.md) › [writer](README.md)

# sgcl::compress::lzw::writer::write, async_write

```cpp
expected<size_t, io::error> write(const slice<const byte>& data);                         // (1)
async::task<expected<size_t, io::error>> async_write(slice<const byte> data) noexcept;    // (2)
```

Codes `data`, 64 KB at a time, and writes the codes to `out` as they come. A byte the literal width cannot hold (5
with a width of 2) fails the write with `errc::invalid_argument`, after the bytes before it were coded, and the error
is kept for good.

1. Blocks the calling thread for the writes of `out`.
2. Returns a task that does the same, gives the worker back while `out` writes, and yields between the pieces. The
   bytes are read when the task runs: `data` lives until the task is done.

The text forms (a string, a literal, a `std::string_view`) and one byte come from
[mixin::writer](../../io/mixin/writer/README.md).

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to compress, each below 2^`literal_width` |

## Return value

The number of bytes taken, `data.size()`, or the writer's error: a failure of `out`, a byte past the literal width
(an `io::error` of `errc::invalid_argument`), a write after the close (`io::errc::closed`, kept as the others), or the error
kept from before.

## Complexity

Linear in the size of `data`.

## Exceptions

- (1) What the `write` of `out` throws; the errors of the stream and of the format are returned.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer sink;
    compress::lzw::writer w(sink, compress::lzw::order::lsb, 2);
    vector<byte> pixels = {byte(0), byte(1), byte(2), byte(3)};
    println("{}", *w.write(pixels));
    pixels.push_back(byte(5));
    println("{}", w.write(pixels).error().message());
    println("{}", w.close().error().message());
}
```

Output:

```text
4
write lzw: invalid argument
write lzw: invalid argument
```

## See also

- [close](close.md)
- [sgcl::compress::lzw::writer](README.md)
