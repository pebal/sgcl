[sgcl](../../README.md) › [core](../README.md) › [priority_queue](../priority_queue.md)

# sgcl::priority_queue\<T, Container, Compare\>::pop

```cpp
void pop() noexcept(noexcept(c.pop_back()) &&
                    std::is_nothrow_move_constructible_v<value_type> &&
                    std::is_nothrow_move_assignable_v<value_type>);
```

Removes the largest element: `std::pop_heap` moves it to the back of the container, restoring the heap over the
others, and `c.pop_back()` destroys it there. The priority queue must not be empty.

## Parameters

None.

## Return value

None. The element is read with [top](top.md) before the pop.

## Complexity

Logarithmic in the size: at most two comparisons per level of the heap.

## Exceptions

What the move constructor and the move assignment of `T` throw; none when they are noexcept.

If a move throws, the elements stay valid, in an order that may no longer be a heap.

## Notes

`pop` on an empty priority queue is undefined behaviour, as with `std::priority_queue`. When the element is a
`tracked_ptr`, the pop destroys the pointer, not the object: the object lives on while something else refers to
it, and is the collector's when nothing does.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    priority_queue<int> pq(std::less<int>(), vector{5, 1, 4, 1, 3});
    vector<int> sorted;
    while (!pq.empty()) {
        sorted.push_back(pq.top());
        pq.pop();
    }
    println("{}", sorted);
}
```

Output:

```text
[5, 4, 3, 1, 1]
```

## See also

- [top](top.md): access the largest element
- [push](push.md): inserts an element
- [sgcl::priority_queue\<T, Container, Compare\>](../priority_queue.md)
