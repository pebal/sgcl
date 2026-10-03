[sgcl](../../README.md) › [io](../README.md) › [shared_memory](../shared_memory.md)

# sgcl::io::shared_memory::shared_memory

```cpp
shared_memory() noexcept = default;
```

Makes an empty handle, which holds no region (`!s`). The handle of a region is made by [create](create.md) or
[open](open.md); the copy and the assignment are the implicit ones, which copy the word, and the copies are the same
region.

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
    io::shared_memory none;
    (void)io::shared_memory::remove("sgcl-example-constructor");
    io::shared_memory made = io::shared_memory::create("sgcl-example-constructor", 64);
    io::shared_memory copy = made;
    println("{} {} {}", bool(none), bool(made), copy == made);
    (void)io::shared_memory::remove("sgcl-example-constructor");
}
```

Output:

```text
false true true
```

## See also

- [create](create.md), [open](open.md): make the region
- [operator bool](operator_bool.md): whether the handle holds a region
- [sgcl::io::shared_memory](../shared_memory.md)
