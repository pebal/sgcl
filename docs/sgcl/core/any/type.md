[sgcl](../../README.md) › [core](../README.md) › [any](README.md)

# sgcl::any::type

```cpp
const std::type_info& type() const noexcept;
```

The type of the value held: the `typeid` of the decayed type it was constructed as, whichever of the three places
it lies in; `typeid(void)` when the `any` is empty.

## Parameters

None.

## Return value

The `std::type_info` of the value, or of `void` when there is none.

## Complexity

Constant.

## Exceptions

None.

## Notes

[any_cast](any_cast.md) compares this type with the one it is asked for; a value held as `tracked_ptr<Node>` is not
found by `any_cast<tracked_ptr<const Node>>`, as with `std::any`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <typeinfo>

using namespace sgcl;

int main() {
    any a;
    println("{}", a.type() == typeid(void));
    a = 1;
    println("{}", a.type() == typeid(int));
    a = "text";  // a const char*, decayed from the array
    println("{} {}", a.type() == typeid(const char*), a.type() == typeid(string));
}
```

Output:

```text
true
true
true false
```

## See also

- [any_cast](any_cast.md): the value as a given type
- [has_value](has_value.md): checks whether the `any` holds a value
- [sgcl::any](README.md)
