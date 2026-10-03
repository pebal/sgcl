[sgcl](../../README.md) › [concurrent](../README.md) › [priority_queue](../priority_queue.md)

# sgcl::concurrent::priority_queue\<T, Compare\>::value_comp

```cpp
value_compare value_comp() const noexcept(std::is_nothrow_copy_constructible_v<Compare>);
```

A copy of the comparator the queue orders its elements by: `comp(a, b)` is `true` when `a` comes out before `b`.

## Parameters

None.

## Return value

A copy of the comparator.

## Complexity

Constant.

## Exceptions

What the copy of `Compare` throws; none when it is noexcept.

## Notes

The comparator never changes after the construction, so the copy is taken without the lock.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>

using namespace sgcl;

int main() {
    concurrent::priority_queue<int, std::greater<int>> largest;
    auto comes_first = largest.value_comp();
    println("{} {}", comes_first(5, 3), comes_first(3, 5));
}
```

Output:

```text
true false
```

## See also

- [(constructor)](priority_queue.md): sets the comparator
- [sgcl::concurrent::priority_queue\<T, Compare\>](../priority_queue.md)
