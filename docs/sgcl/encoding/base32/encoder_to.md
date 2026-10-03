[sgcl](../../README.md) › [encoding](../README.md) › [base32](../base32.md)

# sgcl::encoding::base32::encoder_to

```cpp
encoder encoder_to(const io::writer& out) const noexcept;
```

A writer that writes the encoding of what it is given to `out`: Go's `NewEncoder`. Whole groups of five bytes go
out at once, the bytes short of a group wait for the next write, and
[close()](../base32-encoder/close.md) writes the last group with its padding and leaves `out` open, as Go's does.
The [encoder](../base32-encoder.md) is a handle of one word, made with its state: a managed object holding an
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
    encoding::base32::encoder b32 = encoding::base32::hex.encoder_to(out);
    b32.write("foo");
    b32.write("bar");
    b32.close();
    println(out.text());
}
```

Output:

```text
CPNMUOJ1E8======
```

## See also

- [base32::encoder](../base32-encoder.md): the stream
- [decoder_from](decoder_from.md): the other way
- [encode](encode.md): the text at once
- [sgcl::encoding::base32](../base32.md)
