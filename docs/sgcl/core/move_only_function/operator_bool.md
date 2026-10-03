[sgcl](../../README.md) › [core](../README.md) › [move_only_function](../move_only_function.md)

# sgcl::move_only_function\<R(Args...)\>::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the `move_only_function` holds a callable. It is empty when default-constructed, made from `nullptr`,
a null pointer or an empty function, moved from, or assigned `nullptr`.

## Parameters

None.

## Return value

`true` when there is a callable, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Notes

Calling an empty `move_only_function` is undefined: a call that may find one empty asks this first.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <utility>

using namespace sgcl;

int main() {
    move_only_function<void()> f = [] {};
    move_only_function<void()> g = std::move(f);
    println("{} {}", bool(f), bool(g));
}
```

Output:

```text
false true
```

## See also

- [operator()](operator_call.md): calls the callable
- [sgcl::move_only_function\<R(Args...)\>](../move_only_function.md)
