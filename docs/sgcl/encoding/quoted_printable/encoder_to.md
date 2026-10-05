[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](README.md)

# sgcl::encoding::quoted_printable::encoder_to

```cpp
encoder encoder_to(const io::writer& out) const noexcept;
```

A writer that encodes what is written to it into `out`, in this codec's form: Go's `quotedprintable.NewWriter`.
The bytes go out as they are encoded, but a space or a tab, which waits for the byte after it (it is escaped
before a line break); [close](../quoted_printable-encoder/close.md) writes what waits and leaves `out` open.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the text goes: any writer (a file, a buffer, a connection) |

## Return value

The [encoder](../quoted_printable-encoder/README.md), a handle of the new stream.

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
    auto enc = encoding::quoted_printable::standard.encoder_to(out);
    enc.write("Zażółć ");
    enc.write("gęślą\n");
    enc.close();
    print("{}", out.text());
}
```

Output:

```text
Za=C5=BC=C3=B3=C5=82=C4=87 g=C4=99=C5=9Bl=C4=85
```

## See also

- [quoted_printable::encoder](../quoted_printable-encoder/README.md)
- [encode](encode.md): the text at once
- [quoted_printable](README.md)
