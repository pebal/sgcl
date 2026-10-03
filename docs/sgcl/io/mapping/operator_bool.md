[sgcl](../../README.md) › [io](../README.md) › [mapping](../mapping.md)

# sgcl::io::mapping::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a mapping: one made by [map](../map.md) does, a default-constructed one does not. A
closed mapping is still held: [is_closed](is_closed.md) says it is closed.

## Parameters

None.

## Return value

`true` when the handle holds a mapping, `false` for an empty one.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::mapping m;
    println("{}", bool(m));
    (void)io::write_file("data.txt", string("data"));
    m = io::map("data.txt");
    (void)m.close();
    println("{}", bool(m));
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](mapping.md): an empty handle
- [is_closed](is_closed.md): whether the mapping was closed
- [sgcl::io::mapping](../mapping.md)
