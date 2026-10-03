[sgcl](../../README.md) › [core](../README.md) › [slice](../slice.md)

# sgcl::as_bytes, sgcl::as_writable_bytes (sgcl::slice)

```cpp
#include "sgcl/core/slice.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    slice<const byte> as_bytes(const slice<T>& s) noexcept;       // (1)
    template<class T>
    requires (!std::is_const_v<T>)
    slice<byte> as_writable_bytes(const slice<T>& s) noexcept;    // (2)
}
```

The bytes of the elements of `s`, `s.size_bytes()` of them, as a slice of the same owner, as `std::as_bytes` and
`std::as_writable_bytes` give them of a span.

1. The bytes to read.
2. The bytes to write, for a slice whose elements are not `const`.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the slice whose bytes are taken |

## Return value

A slice of the bytes, with the owner of `s`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <cstdint>

using namespace sgcl;

int main() {
    vector<std::uint16_t> words = {0x0102, 0x0304};
    slice<std::uint16_t> s = words;
    slice<byte> raw = as_writable_bytes(s);
    raw.fill(byte{0});
    println("{} {} {}", as_bytes(s).size(), as_bytes(s).owned(), words);
}
```

Output:

```text
4 true [0, 0]
```

## See also

- [size_bytes](size_bytes.md): the size of the elements in bytes
- [sgcl::slice\<T\>](../slice.md)
