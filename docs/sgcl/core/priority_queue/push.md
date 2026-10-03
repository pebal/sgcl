[sgcl](../../README.md) › [core](../README.md) › [priority_queue](README.md)

# sgcl::priority_queue\<T, Container, Compare\>::push

```cpp
void push(const value_type& value)                                  // (1)
    noexcept(noexcept(c.push_back(value)) &&
             std::is_nothrow_move_constructible_v<value_type> &&
             std::is_nothrow_move_assignable_v<value_type>);
void push(value_type&& value)                                       // (2)
    noexcept(noexcept(c.push_back(std::move(value))) &&
             std::is_nothrow_move_constructible_v<value_type> &&
             std::is_nothrow_move_assignable_v<value_type>);
```

Inserts an element: appends it to the container, `c.push_back(value)`, and sifts it up the heap with
`std::push_heap`.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to insert |

## Return value

None.

## Complexity

Logarithmic in the size: at most one comparison per level of the heap, plus the container's `push_back`
(amortized constant for `vector`).

## Exceptions

What the copy or the move constructor of `T` throws, and its move assignment while the element is sifted up;
none when they are noexcept.

If the construction of the element throws, the container's `push_back` leaves the priority queue as it was, for
`vector` and `deque`. If a move of the sift throws, the elements stay valid, in an order that may no longer be a
heap.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    priority_queue<int> pq;
    for (int x : {3, 1, 4, 1, 5}) {
        pq.push(x);
        print("{} ", pq.top());
    }
    println("({} elements)", pq.size());
}
```

Output:

```text
3 3 4 4 5 (5 elements)
```

## See also

- [emplace](emplace.md): constructs the element in place
- [pop](pop.md): removes the largest element
- [sgcl::priority_queue\<T, Container, Compare\>](README.md)
