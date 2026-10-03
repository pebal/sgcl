[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex.md)

# sgcl::encoding::hex::encoder_to

```cpp
static encoder encoder_to(const io::writer& out) noexcept;
```

A writer that writes the lower-case digits of what it is given to `out`: Go's `hex.NewEncoder`. A byte is a whole
group, so every write goes out at once and nothing waits; [close()](../hex-encoder/close.md) ends the encoder and
leaves `out` open. The [encoder](../hex-encoder.md) is a handle of one word, made with its state: a managed object
holding an 8 KB block and `out`.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the digits go to: any stream of io, a handle of the library, a stream of one's own |

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
    encoding::hex::encoder digits = encoding::hex::encoder_to(out);
    digits.write("Hi");
    println("{}", out.text());
    digits.write("!");
    digits.close();
    println("{}", out.text());
}
```

Output:

```text
4869
486921
```

## See also

- [hex::encoder](../hex-encoder.md): the stream
- [decoder_from](decoder_from.md): the other way
- [encode](encode.md): the digits at once
- [sgcl::encoding::hex](../hex.md)
