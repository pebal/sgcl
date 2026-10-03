[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](README.md)

# sgcl::encoding::ascii85::decoder_from

```cpp
static decoder decoder_from(const io::reader& in) noexcept;
```

A reader of the bytes the Ascii85 text of `in` decodes to: Go's `ascii85.NewDecoder`. The text may come in pieces
of any size, and is read as [decode](decode.md) reads it. An invalid text fails the read that reaches it, after
the bytes before the error were handed out, and every read after; the [decoder](../ascii85-decoder/README.md)'s
[last_error()](../ascii85-decoder/last_error.md) holds the [error](../error/README.md) with its offset in the text. The
decoder is a handle of one word, made with its state: a managed object holding an 8 KB block and `in`.

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
    text.write("87cURD_*#4\nDfTZ)+T");
    encoding::ascii85::decoder plain = encoding::ascii85::decoder_from(text);
    println(plain.read_all_text().value());
}
```

Output:

```text
Hello, World!
```

## See also

- [ascii85::decoder](../ascii85-decoder/README.md): the stream
- [encoder_to](encoder_to.md): the other way
- [decode](decode.md): the bytes at once
- [sgcl::encoding::ascii85](README.md)
