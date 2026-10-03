[sgcl](../../README.md) › [core](../README.md) › [function](../function.md)

# sgcl::swap (sgcl::function)

```cpp
#include "sgcl/core/function.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class R, class... Args>
    void swap(function<R(Args...)>& l, function<R(Args...)>& r) noexcept;
}
```

Swaps the callables of `l` and `r`: `l.swap(r)` ([swap](swap.md)).

## Parameters

| Parameter | Description |
|---|---|
| `l`, `r` | the `function` objects to swap |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

Found by the argument's type: `swap(a, b)` written without a namespace, and the `using std::swap; swap(a, b);` of
generic code, call it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    function<string()> a = [] { return string("a"); };
    function<string()> b;
    swap(a, b);
    println("{} {}", bool(a), b());
}
```

Output:

```text
false a
```

## See also

- [swap](swap.md): the member function
- [sgcl::function\<R(Args...)\>](../function.md)
