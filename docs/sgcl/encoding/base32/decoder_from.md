[sgcl](../../README.md) › [encoding](../README.md) › [base32](README.md)

# sgcl::encoding::base32::decoder_from

```cpp
decoder decoder_from(const io::reader& in) const noexcept;
```

A reader of the bytes the text of `in` decodes to: Go's `NewDecoder`. The text may come in pieces of any size,
and is read as [decode](decode.md) reads it, strict or lenient as the codec is. An invalid text fails the read
that reaches it, after the bytes before the error were handed out, and every read after; the
[decoder](../base32-decoder/README.md)'s [last_error()](../base32-decoder/last_error.md) holds the
[error](../error/README.md) with its offset in the text. The decoder is a handle of one word, made with its state: a
managed object holding an 8 KB block and `in`.

## Parameters

| Parameter | Description |
|---|---|
| `in` | the reader of the text: any stream of io, a handle of the library, a stream of one's own |

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
    io::buffer text;
    text.write("MZXW6YTBOI======");
    encoding::base32::decoder plain = encoding::base32::standard.decoder_from(text);
    println(plain.read_all_text().value());
}
```

Output:

```text
foobar
```

## See also

- [base32::decoder](../base32-decoder/README.md): the stream
- [encoder_to](encoder_to.md): the other way
- [decode](decode.md): the bytes at once
- [sgcl::encoding::base32](README.md)
