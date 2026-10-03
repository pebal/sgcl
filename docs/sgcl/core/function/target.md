[sgcl](../../README.md) › [core](../README.md) › [function](README.md)

# sgcl::function\<R(Args...)\>::target

```cpp
template<class T> T* target() noexcept;                // (1)
template<class T> const T* target() const noexcept;    // (2)
```

A pointer to the callable held, when its type is `T` (compared by `typeid`, as [target_type](target_type.md) gives
it); null when it is of another type or the `function` is empty.

## Parameters

None.

## Return value

A pointer to the callable, into the `function` or into the node that holds it, or `nullptr`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The pointer is valid while the `function` holds that callable: an assignment destroys it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int twice(int x) {
    return x * 2;
}

int main() {
    function<int(int)> f = twice;
    auto p = f.target<int (*)(int)>();
    println("{} {}", p != nullptr, (*p)(4));
    println("{}", f.target<int (*)(long)>() == nullptr);
}
```

Output:

```text
true 8
true
```

## See also

- [target_type](target_type.md): the type of the callable
- [sgcl::function\<R(Args...)\>](README.md)
