[sgcl](../../README.md) › [encoding](../README.md) › [base64](../base64.md)

# sgcl::encoding::base64::encoder_to

```cpp
encoder encoder_to(const io::writer& out) const noexcept;
```

A writer that writes the encoding of what it is given to `out`: Go's `NewEncoder`. Whole groups go out at once,
the bytes short of a group wait for the next write, and [close()](../base64-encoder/close.md) writes the last
group with its padding and leaves `out` open, as Go's does, since what is written around the base64 usually goes
on. The [encoder](../base64-encoder.md) is a handle of one word, made with its state: a managed object holding an
8 KB block and `out`.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the text goes to: any stream of io, a handle of the library, a stream of one's own |

## Return value

The encoder.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer out;
    out.write("data:");
    encoding::base64::encoder armored = encoding::base64::standard.encoder_to(out);
    armored.write("hello, ");
    armored.write("world");
    armored.close();
    out.write(";");
    println(out.text());
}
```

Output:

```text
data:aGVsbG8sIHdvcmxk;
```

## See also

- [base64::encoder](../base64-encoder.md): the stream
- [decoder_from](decoder_from.md): the other way
- [encode](encode.md): the text at once
- [sgcl::encoding::base64](../base64.md)
