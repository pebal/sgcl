[sgcl](../../README.md) › [core](../README.md) › [function](../function.md)

# sgcl::function\<R(Args...)\>::target_type

```cpp
const std::type_info& target_type() const noexcept;
```

The type of the callable held: the `typeid` of the decayed type it was given as, wherever it lies; `typeid(void)`
when the `function` is empty.

## Parameters

None.

## Return value

The `std::type_info` of the callable, or of `void`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <typeinfo>

using namespace sgcl;

int twice(int x) {
    return x * 2;
}

int main() {
    function<int(int)> f;
    println("{}", f.target_type() == typeid(void));
    f = twice;
    println("{}", f.target_type() == typeid(int (*)(int)));
}
```

Output:

```text
true
true
```

## See also

- [target](target.md): a pointer to the callable, by its type
- [sgcl::function\<R(Args...)\>](../function.md)
