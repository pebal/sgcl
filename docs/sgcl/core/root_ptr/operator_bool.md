[sgcl](../../README.md) › [core](../README.md) › [root_ptr](README.md)

# sgcl::root_ptr\<T\>::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the root holds a pointer that is not null.

## Parameters

None.

## Return value

`true` when the pointer is not null, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    root_ptr<int> slot;  // a cell, null inside
    println("{}", bool(slot));
    slot = make_tracked<int>(4);
    if (slot) {
        println("{}", *slot);
    }
}
```

Output:

```text
false
4
```

## See also

- [get](get.md): the raw pointer
- [sgcl::root_ptr\<T\>](README.md)
