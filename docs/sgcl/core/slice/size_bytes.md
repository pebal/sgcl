[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::size_bytes

```cpp
size_type size_bytes() const noexcept;
```

The size of the elements in bytes: `size() * sizeof(T)`, as `std::span::size_bytes`.

## Parameters

None.

## Return value

The number of bytes the elements take.

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
    vector<std::uint32_t> words(4);
    slice<std::uint32_t> s = words;
    println("{} {}", s.size(), s.size_bytes());
}
```

Output:

```text
4 16
```

## See also

- [as_bytes, as_writable_bytes](as_bytes.md): the bytes as a slice
- [sgcl::slice\<T\>](README.md)
