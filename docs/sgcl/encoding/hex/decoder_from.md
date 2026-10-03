[sgcl](../../README.md) › [encoding](../README.md) › [hex](README.md)

# sgcl::encoding::hex::decoder_from

```cpp
static decoder decoder_from(const io::reader& in) noexcept;
```

A reader of the bytes the digits of `in` decode to: Go's `hex.NewDecoder`. The digits may come in pieces of any
size, a byte's two digits split between two of them included, and are read as [decode](decode.md) reads them. An
invalid text fails the read that reaches it, after the bytes before the error were handed out, and every read
after; the [decoder](../hex-decoder/README.md)'s [last_error()](../hex-decoder/last_error.md) holds the
[error](../error/README.md) with its offset in the text. The decoder is a handle of one word, made with its state: a
managed object holding an 8 KB block and `in`.

## Parameters

| Parameter | Description |
|---|---|
| `in` | the reader of the digits: any stream of io, a handle of the library, a stream of one's own |

## Return value

The decoder.

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
    io::buffer digits;
    digits.write("48656c6c6f");
    encoding::hex::decoder plain = encoding::hex::decoder_from(digits);
    println(plain.read_all_text().value());
}
```

Output:

```text
Hello
```

## See also

- [hex::decoder](../hex-decoder/README.md): the stream
- [encoder_to](encoder_to.md): the other way
- [decode](decode.md): the bytes at once
- [sgcl::encoding::hex](README.md)
