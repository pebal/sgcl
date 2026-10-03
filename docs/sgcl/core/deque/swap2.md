[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::swap (sgcl::deque)

```cpp
#include "sgcl/core/deque.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    void swap(deque<T>& lhs, deque<T>& rhs) noexcept;
}
```

Exchanges the contents of `lhs` and `rhs`: `lhs.swap(rhs)`. No element is touched.

## Parameters

| Parameter | Description |
|---|---|
| `lhs`, `rhs` | the deques to exchange the contents of |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

The overload is found by the argument's type: `swap(a, b)` written without a namespace calls it. The iterators and references stay valid and refer to the same
elements, now in the other deque.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque<string> today = {"mail", "review"};
    deque<string> tomorrow = {"deploy"};
    swap(today, tomorrow);
    println("{} {}", today, tomorrow);
}
```

Output:

```text
["deploy"] ["mail", "review"]
```

## See also

- [swap](swap.md): the member swap
- [sgcl::deque\<T\>](../deque.md)
