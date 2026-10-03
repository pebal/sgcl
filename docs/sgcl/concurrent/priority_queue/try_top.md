[sgcl](../../README.md) › [concurrent](../README.md) › [priority_queue](../priority_queue.md)

# sgcl::concurrent::priority_queue\<T, Compare\>::try_top

```cpp
optional<T> try_top() const
    noexcept(std::is_nothrow_copy_constructible_v<T>) requires std::is_copy_constructible_v<T>;
```

A copy of the least element, taken under the lock, or nothing when the queue is empty; the queue is not changed.
Takes part only when `T` is copy-constructible.

## Parameters

None.

## Return value

A copy of the least element, or `nullopt` when the queue was empty.

## Complexity

Constant, under the lock, plus the copy of the element.

## Exceptions

What the copy constructor of `T` throws; none when it is noexcept. The queue is unchanged.

## Notes

There is no `top()` by reference: another thread may pop the element the moment the lock is released, and the
copy is what stays valid. The element may be gone by the time the copy is read; a consumer takes the element with
[try_pop](try_pop.md) instead of looking first.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::priority_queue<int> numbers = {5, 2, 8};
    if (auto least = numbers.try_top()) {
        println("{}, {} elements", *least, numbers.size());
    }

    concurrent::priority_queue<int> empty;
    println("{}", empty.try_top());
}
```

Output:

```text
2, 3 elements
nullopt
```

## See also

- [try_pop](try_pop.md): takes the least element
- [sgcl::concurrent::priority_queue\<T, Compare\>](../priority_queue.md)
