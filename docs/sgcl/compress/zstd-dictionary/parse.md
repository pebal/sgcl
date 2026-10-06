[sgcl](../../README.md) › [compress](../README.md) › [zstd](../zstd/README.md) › [dictionary](README.md)

# sgcl::compress::zstd::dictionary::parse

```cpp
static expected<dictionary, error> parse(const slice<const byte>& bytes) noexcept;
```

Reads a dictionary from `bytes`, which are copied: zstd's format when they begin with its magic (0xEC30A437 in little
endian), its id, its Huffman table, its three FSE tables and its three repeated offsets checked, the rest its content;
any other bytes as raw content, id 0, never refused. Empty bytes are no dictionary. For bytes from outside the program
(a dictionary downloaded or read from a file).

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | a dictionary of zstd's format, or any bytes as raw content |

## Return value

The dictionary, or the [error](../error/README.md) (`errc::corrupt`) when the bytes begin with the magic and a table is
not valid, an offset is 0 or past the content, or the bytes end inside the tables.

## Complexity

Linear in the size of `bytes`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto raw = compress::zstd::dictionary::parse(slice<const byte>(string("any bytes at all")));
    println("{} {}", raw.has_value(), raw->size());
    auto damaged = compress::zstd::dictionary::parse(slice<const byte>(string("\x37\xa4\x30\xec" "0000")));
    println("{}", damaged.error().message());
}
```

Output:

```text
true 16
offset 0: zstd: a dictionary's Huffman table that is not valid
```

## See also

- [(constructor)](zstd-dictionary.md): throwing instead
- [sgcl::compress::zstd::dictionary](README.md)
