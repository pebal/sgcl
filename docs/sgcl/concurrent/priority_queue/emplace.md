[sgcl](../../README.md) › [concurrent](../README.md) › [priority_queue](../priority_queue.md)

# sgcl::concurrent::priority_queue\<T, Compare\>::emplace

```cpp
template<class... A>
void emplace(A&&... a)
    noexcept(std::is_nothrow_constructible_v<T, A...> && std::is_nothrow_move_constructible_v<T> &&
             std::is_nothrow_move_assignable_v<T>);
```

Inserts an element constructed from `a...`, `T(std::forward<A>(a)...)`. Under the lock, the element is
constructed, takes the next number of the queue's counter, is appended to the heap's vector and sifted up; after
the lock, the count of the pushes is raised and a thread waiting in [pop](pop.md) is woken when there is one.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the element is constructed from |

## Return value

None.

## Complexity

Logarithmic in the size: at most log *n* comparisons, under the lock; amortized constant for the growth of the
heap's vector.

## Exceptions

What the constructor of `T`, and the move constructor and the move assignment of `T`, throw; none when they
are noexcept.

When the constructor of `T` throws, the queue is as it was: the element is not in it, the others keep their order,
`size()` is unchanged and no thread waiting in [pop](pop.md) is woken. A move of `T` that throws while the element
is moved into its place leaves the heap's contents unspecified; the move should not throw.

## Notes

The element is constructed under the lock: a constructor that takes long holds the other threads up for its
length, and one that is costly is better run before, by [push](push.md) of a value made outside.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Order {
    int priority;
    string item;
    bool operator<(const Order& other) const { return priority < other.priority; }
};

int main() {
    concurrent::priority_queue<Order> orders;
    orders.emplace(3, "lamp");
    orders.emplace(1, "desk");
    orders.emplace(2, "chair");

    while (auto order = orders.try_pop()) {
        println("{} {}", order->priority, order->item);
    }
}
```

Output:

```text
1 desk
2 chair
3 lamp
```

## See also

- [push](push.md): inserts a copy or a moved value
- [try_pop](try_pop.md): takes the least element
- [sgcl::concurrent::priority_queue\<T, Compare\>](../priority_queue.md)
