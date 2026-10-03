[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex.md)

# sgcl::encoding::hex::dump

```cpp
static string dump(const slice<const byte>& data);    // (1)
static string dump(const string& text);               // (2)
template<class T>
static string dump(const T& text);                    // (3)
```

The lines `hexdump -C` writes of bytes, as Go's `hex.Dump` writes them, line for line: the offset in eight
hexadecimal digits (more past 4 GB), two spaces, sixteen bytes in two columns of eight, and the bytes as
characters between bars, a dot for what is not printable ASCII. The short line at the end keeps the columns where
they are. There is no closing line with the total that `hexdump -C` writes. Every line ends in `'\n'`.

1. The dump of `data`.
2. The dump of the bytes of `text`.
3. The same as (2) for a literal, a character array or a `std::string_view`, read where it lies; it takes part
   only for those.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to dump |
| `text` | the text whose bytes are dumped |

## Return value

The lines, empty for no bytes.

## Complexity

Linear in the size of the input.

## Exceptions

`length_error` when the dump would be longer than a string holds (4 G characters).

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    print(encoding::hex::dump("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n"));
}
```

Output:

```text
00000000  47 45 54 20 2f 20 48 54  54 50 2f 31 2e 31 0d 0a  |GET / HTTP/1.1..|
00000010  48 6f 73 74 3a 20 65 78  61 6d 70 6c 65 2e 63 6f  |Host: example.co|
00000020  6d 0d 0a 0d 0a                                    |m....|
```

## See also

- [dumper_to](dumper_to.md): the dump as a stream
- [encode](encode.md): the digits alone
- [sgcl::encoding::hex](../hex.md)
