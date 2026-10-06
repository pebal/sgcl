[sgcl](../../README.md) › [codec](../README.md) › [qr](README.md)

# sgcl::codec::qr::correction

```cpp
level correction() const noexcept;
```

The level of error correction written into the symbol: the one asked, or a higher one where the options let [encode](encode.md) raise it at no cost in size (`boost_level`).

## Parameters

None.

## Return value

The [level](../qr-level.md).

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::qr boosted = codec::qr::encode("SGCL", {.level = codec::qr::level::low});
    codec::qr kept = codec::qr::encode("SGCL", {.level = codec::qr::level::low,
                                                .boost_level = false});
    println("{} {}", int(boosted.correction()), int(kept.correction()));
}
```

Output:

```text
3 0
```

## See also

- [dark](dark.md): the modules
- [sgcl::codec::qr](README.md)
