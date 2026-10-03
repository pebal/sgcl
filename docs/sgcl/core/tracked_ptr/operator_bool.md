[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](../tracked_ptr.md)

# sgcl::tracked_ptr\<T\>::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the pointer is not null.

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
    tracked_ptr<int> none;
    tracked_ptr number = make_tracked<int>(5);
    if (number) {
        println("{}", *number);
    }
    println("{} {}", bool(none), bool(number));
}
```

Output:

```text
5
false true
```

## See also

- [get](get.md): the raw pointer
- [sgcl::tracked_ptr\<T\>](../tracked_ptr.md)
