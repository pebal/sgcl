[sgcl](../../README.md) › [concurrent](../README.md) › [priority_queue](../priority_queue.md)

# sgcl::concurrent::priority_queue\<T, Compare\>::try_pop

```cpp
optional<T> try_pop()
    noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>);
```

Takes the least element, or nothing when the queue is empty. Under the lock, the front of the heap is moved to the
back of its vector and the rest put back in order, in the steps of `std::pop_heap`: the hole at the front moved down
to a leaf along the lesser children, the last element moved into it and sifted up. The least element is then moved
out of the back into the `optional` returned and destroyed there. Of equal elements, the one pushed first is taken.

## Parameters

None.

## Return value

The least element, moved out, or `nullopt` when the queue was empty. `optional` is the alias of `std::optional`
([aliases](../../core/aliases.md)).

## Complexity

Logarithmic in the size: at most 2 log *n* comparisons, under the lock.

## Exceptions

What the move constructor and the move assignment of `T` throw; none when they are noexcept.

A move of `T` that throws leaves the heap's contents unspecified; the move should not throw.

## Notes

Takes the lock, as every pop and push does; `try_pop` never waits for an element, only for the lock, which it
spins through for the length of another thread's operation.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::priority_queue<int> numbers = {5, 2, 8};

    vector<int> taken;
    while (auto n = numbers.try_pop()) {
        taken.push_back(*n);
    }
    println("{}", taken);
    println("{}", numbers.try_pop());
}
```

Output:

```text
[2, 5, 8]
nullopt
```

## See also

- [pop](pop.md): waits for an element
- [try_top](try_top.md): a copy of the least element, the queue unchanged
- [sgcl::concurrent::priority_queue\<T, Compare\>](../priority_queue.md)
