[sgcl](../../README.md) › [io](../README.md) › [shared_memory](README.md)

# sgcl::io::shared_memory::is_closed

```cpp
bool is_closed() const noexcept;
```

Checks whether the region was [closed](close.md), through this handle or any copy of it: the copies are one region.
Another [open](open.md) of the name is another region, which the close does not reach.

## Parameters

None.

## Return value

`true` once the region is closed.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::shared_memory::remove("sgcl-example-closed");
    io::shared_memory made = io::shared_memory::create("sgcl-example-closed", 64);
    io::shared_memory copy = made;
    io::shared_memory opened = io::shared_memory::open("sgcl-example-closed");
    (void)copy.close();
    println("{} {}", made.is_closed(), opened.is_closed());
    (void)io::shared_memory::remove("sgcl-example-closed");
}
```

Output:

```text
true false
```

## See also

- [close](close.md): gives the region back
- [operator bool](operator_bool.md): whether the handle holds a region at all
- [sgcl::io::shared_memory](README.md)
