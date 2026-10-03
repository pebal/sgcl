[sgcl](../../README.md) › [io](../README.md) › [mapping](README.md)

# sgcl::io::mapping::is_closed

```cpp
bool is_closed() const noexcept;
```

Checks whether the mapping was [closed](close.md), through this handle or any copy of it: the copies are one
mapping.

## Parameters

None.

## Return value

`true` once the mapping is closed.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("shared.txt", string("shared"));
    io::mapping a = io::map("shared.txt");
    io::mapping b = a;
    println("{}", a.is_closed());
    (void)b.close();
    println("{}", a.is_closed());
}
```

Output:

```text
false
true
```

## See also

- [close](close.md): gives the file back
- [operator bool](operator_bool.md): whether the handle holds a mapping at all
- [sgcl::io::mapping](README.md)
