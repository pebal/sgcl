[sgcl](../../README.md) › [core](../README.md) › [function](../function.md)

# sgcl::operator== (sgcl::function)

```cpp
#include "sgcl/core/function.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class R, class... Args>
    bool operator==(const function<R(Args...)>& f, std::nullptr_t) noexcept;
}
```

Checks whether `f` is empty: `!f`. `nullptr == f`, `f != nullptr` and `nullptr != f` are rewritten to it by the
compiler. Two `function` objects do not compare, as in `std`.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the `function` to check |

## Return value

`true` when `f` holds no callable, `false` otherwise.

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
    function<void()> f;
    println("{}", f == nullptr);
    f = [] {};
    println("{} {}", f == nullptr, nullptr != f);
}
```

Output:

```text
true
false true
```

## See also

- [operator bool](operator_bool.md): the same question
- [sgcl::function\<R(Args...)\>](../function.md)
