[sgcl](../../README.md) › [compress](../README.md) › [zstd](../zstd/README.md) › [dictionary](README.md)

# sgcl::compress::zstd::dictionary::dictionary

```cpp
dictionary() noexcept;                                  // (1)
explicit dictionary(const slice<const byte>& bytes);    // (2)
```

1. No dictionary: options holding it compress and read without one.
2. The dictionary `bytes` hold, as [parse](parse.md) reads it: zstd's format when they begin with its magic, raw
   content otherwise; the bytes are copied. For bytes the program trusts (a dictionary shipped with it): a damaged one
   throws, where [parse](parse.md) gives the error to handle.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | a dictionary of zstd's format, or any bytes as raw content; none when empty |

## Complexity

1. Constant.
2. Linear in the size of `bytes`.

## Exceptions

- (1) None.
- (2) `bad_expected_access<compress::error>` when the bytes begin with zstd's magic and the dictionary is damaged
  (`errc::corrupt`).

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::zstd::dictionary none;
    compress::zstd::dictionary common(slice<const byte>(string("the words messages share")));
    println("{} {}", none.empty(), common.size());
    try {
        compress::zstd::dictionary broken(slice<const byte>(string("\x37\xa4\x30\xec" "garbage")));
    } catch (const bad_expected_access<compress::error>& e) {
        println("{}", e.error().message());
    }
}
```

Output:

```text
true 24
offset 0: zstd: a dictionary's Huffman table that is not valid
```

## See also

- [parse](parse.md): the error to handle instead
- [sgcl::compress::zstd::dictionary](README.md)
