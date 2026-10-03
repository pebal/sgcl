[sgcl](../../README.md) › [io](../README.md) › [buffer](../buffer.md)

# sgcl::io::buffer::write, async_write

```cpp
expected<size_t, error> write(const slice<const byte>& in) const;                // (1)
async::task<expected<size_t, error>> async_write(const slice<const byte>& in)    // (2)
    const noexcept;
```

Writes the bytes of `in` at the write position. At the end, where the position is until a [seek](seek.md) moves
it, the bytes are appended. Elsewhere they overwrite what is held from the position on and run on past the end; a
position past the end, which a seek may set, is reached by filling the gap with zeros first, as `pwrite` does. The
position moves past the bytes written. `in` may be bytes of the buffer itself, its [data](data.md) or a part of it:
what is written is those bytes as they were before the write, as `pwrite` writes what it was given.

1. Writes at once.
2. Returns a task that writes at its first step: it never waits.

The class brings in the overloads of [mixin::writer](../mixin/writer/write.md) as well, which write a text (a
string, a text slice, a literal, a C string, a `std::string_view`) or one byte through these.

## Parameters

| Parameter | Description |
|---|---|
| `in` | the bytes to write |

## Return value

`in.size()`. Never an error.

## Complexity

Linear in the bytes written, amortized at the end: the vector grows by doubling. Plus the zeros of a gap.

## Exceptions

- (1) `length_error` when the buffer would pass the largest size of a vector, which only a write after a seek far
  past the end reaches; the buffer is as it was.
- (2) None. What the write throws is the task's, rethrown by the `co_await`.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer out;
    out.write(vector<byte>{byte('a'), byte('b'), byte('c')});
    out.write("def");
    out.write(byte('!'));
    println("{}", out.text());

    out.seek(1);
    out.write("XY");
    println("{}", out.text());

    out.seek(9);
    out.write("Z");
    println("{} bytes, the gap zeros: {}", out.size(), out.data()[7] == byte(0));
}
```

Output:

```text
abcdef!
aXYdef!
10 bytes, the gap zeros: true
```

## See also

- [seek](seek.md): the write position
- [mixin::writer::write](../mixin/writer/write.md): a text or one byte
- [sgcl::io::buffer](../buffer.md)
