[sgcl](../../README.md) › [io](../README.md) › [shared_memory](../shared_memory.md)

# sgcl::io::shared_memory::size

```cpp
size_t size() const noexcept;
```

Returns the length of the region: the size given to [create](create.md) for the region it made; for one
[open](open.md) mapped, the object's size as the system keeps it, which macOS and Windows round up to a page.

## Parameters

None.

## Return value

The length of the region in bytes; 0 once the region is [closed](close.md).

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::shared_memory::remove("sgcl-example-size");
    io::shared_memory region = io::shared_memory::create("sgcl-example-size", 100);
    println("{}", region.size());
    (void)region.close();
    println("{}", region.size());
    (void)io::shared_memory::remove("sgcl-example-size");
}
```

Output:

```text
100
0
```

## See also

- [data](data.md): the bytes
- [sgcl::io::shared_memory](../shared_memory.md)
