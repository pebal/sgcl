[sgcl](../../README.md) › [io](../README.md) › [mapping](README.md)

# sgcl::io::mapping::mapping

```cpp
mapping() noexcept = default;
```

Makes an empty handle, which holds no mapping (`!m`). The handle of a mapped file is made by [map](../map.md); the
copy and the assignment are the implicit ones, which copy the word, and the copies are the same mapping.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::mapping none;
    (void)io::write_file("data.txt", string("data"));
    io::mapping m = io::map("data.txt");
    io::mapping copy = m;
    println("{} {} {}", bool(none), bool(m), copy == m);
}
```

Output:

```text
false true true
```

## See also

- [map](../map.md): makes the mapping
- [operator bool](operator_bool.md): whether the handle holds a mapping
- [sgcl::io::mapping](README.md)
