[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](../quoted_printable/README.md) › [encoder](README.md)

# sgcl::encoding::quoted_printable::encoder::write, async_write

```cpp
expected<size_t, io::error> write(const slice<const byte>& data) const;                      // (1)
async::task<expected<size_t, io::error>> async_write(const slice<const byte>& data) const    // (2)
    noexcept;
```

Encodes `data` into the writer under the encoder: the text of the bytes goes out at once, but a space or a tab at
the end, which waits for the next byte or for [close](close.md). What is written so is one text, whatever the
pieces.

1. Waits on this thread as the writer under it does.
2. The same in a task, over the writer's `async_write`.

A failure of the writer under it is kept for good; a write after `close()` is `io::errc::closed`. The text and the
byte of the writers of the library, `enc.write("text")`, are [io::mixin::writer](../../io/mixin/writer/README.md)'s.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to encode |

## Return value

`data.size()`, or the `io::error` of the writer under it, or `io::errc::closed`.

## Complexity

Linear in the size of `data`.

## Exceptions

- (1) What the write of the writer under it throws; the writers of the library throw nothing.
- (2) None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    io::buffer out;
    auto enc = encoding::quoted_printable::standard.encoder_to(out);
    println("{}", *enc.write("a = b "));
    println("'{}'", out.text());  // the space waits
    enc.close();
    println("'{}'", out.text());
    println("{}", enc.write("x").error().message());
}
```

Output:

```text
6
'a =3D b'
'a =3D b=20'
write quoted-printable: stream closed
```

## See also

- [close, async_close](close.md)
- [encode](../quoted_printable/encode.md)
- [encoder](README.md)
