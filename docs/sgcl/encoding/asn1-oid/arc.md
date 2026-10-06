[sgcl](../../README.md) › [encoding](../README.md) › [asn1](../asn1/README.md) › [oid](README.md)

# sgcl::encoding::asn1::oid::arc

```cpp
optional<uint64_t> arc(size_t index) const noexcept;
```

The arc at the place, from 0; `nullopt` past the last and for an arc past 64 bits, which
[to_string](to_string.md) writes all the same.

## Parameters

| Parameter | Description |
|---|---|
| `index` | the place |

## Return value

The arc, or `nullopt`.

## Complexity

Linear in the size of the identifier.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1::oid id("2.25.329800735698586629295641978511506172918");
    for (size_t i : range(id.size() + 1)) {
        println("{}: {}", i, id.arc(i));
    }
}
```

Output:

```text
0: 2
1: 25
2: nullopt
3: nullopt
```

## See also

- [size](size.md): how many
- [sgcl::encoding::asn1::oid](README.md)
