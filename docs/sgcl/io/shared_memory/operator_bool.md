[sgcl](../../README.md) › [io](../README.md) › [shared_memory](../shared_memory.md)

# sgcl::io::shared_memory::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a region: one made by [create](create.md) or [open](open.md) does, a
default-constructed one does not. A closed region is still held: [is_closed](is_closed.md) says it is closed.

## Parameters

None.

## Return value

`true` when the handle holds a region, `false` for an empty one.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::shared_memory region;
    println("{}", bool(region));
    (void)io::shared_memory::remove("sgcl-example-bool");
    region = io::shared_memory::create("sgcl-example-bool", 64);
    println("{}", bool(region));
    (void)io::shared_memory::remove("sgcl-example-bool");
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](shared_memory.md): an empty handle
- [is_closed](is_closed.md): whether the region was closed
- [sgcl::io::shared_memory](../shared_memory.md)
