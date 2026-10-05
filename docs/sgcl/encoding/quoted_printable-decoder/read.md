[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](../quoted_printable/README.md) › [decoder](README.md)

# sgcl::encoding::quoted_printable::decoder::read, async_read

```cpp
expected<size_t, io::error> read(const slice<byte>& buffer) const;                      // (1)
async::task<expected<size_t, io::error>> async_read(const slice<byte>& buffer) const    // (2)
    noexcept;
```

The next bytes the text decodes to, into `buffer`; 0 at the end of the text. The text is read from the reader
under the decoder a block of 8 KB at a time.

1. Waits on this thread as the reader under it does.
2. The same in a task.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go |

## Return value

How many bytes, 0 at the end; or an `io::error`: the reader's own, or one of the encoding category for a text a strict decoding refuses ([last_error](last_error.md) has the offset).

## Complexity

Linear in the bytes given.

## Exceptions

- (1) What the read of the reader under it throws; the readers of the library throw nothing.
- (2) None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    io::buffer in;
    in.write("Gr=C3=BC=C3=9Fe");
    auto dec = encoding::quoted_printable::standard.decoder_from(in);
    vector<byte> buf(64);
    size_t n = *dec.read(buf);
    println("{} {}", n, string(buf.as_slice().first(n)));
    println("{}", *dec.read(buf));
}
```

Output:

```text
7 Grüße
0
```

## See also

- [last_error](last_error.md)
- [decode](../quoted_printable/decode.md)
- [decoder](README.md)
