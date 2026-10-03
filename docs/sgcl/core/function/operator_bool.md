[sgcl](../../README.md) › [core](../README.md) › [function](README.md)

# sgcl::function\<R(Args...)\>::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the `function` holds a callable. A `function` is empty when default-constructed, made from `nullptr`,
a null function pointer, a null member pointer or an empty function, moved from, or assigned `nullptr`.

## Parameters

None.

## Return value

`true` when the `function` holds a callable, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>

using namespace sgcl;

int main() {
    function<void()> f;
    std::function<void()> empty_std;
    function<void()> g = empty_std;  // an empty function of std: empty
    function<void()> h = [] {};
    println("{} {} {}", bool(f), bool(g), bool(h));
}
```

Output:

```text
false false true
```

## See also

- [operator==](operator_cmp.md): the same question as `f == nullptr`
- [sgcl::function\<R(Args...)\>](README.md)
