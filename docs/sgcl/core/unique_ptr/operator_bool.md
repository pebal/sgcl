[sgcl](../../README.md) › [core](../README.md) › [unique_ptr](README.md)

# sgcl::unique_ptr\<T\>::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the owner holds an object: the `operator bool` of `std::unique_ptr`.

## Parameters

None.

## Return value

`true` when there is an object, `false` otherwise.

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
    unique_ptr number = make_tracked<int>(5);
    tracked_ptr taken = std::move(number);  // the object goes to the collector
    println("{} {}", bool(number), bool(taken));
}
```

Output:

```text
false true
```

## See also

- [get](get.md): the raw pointer
- [sgcl::unique_ptr\<T\>](README.md)
