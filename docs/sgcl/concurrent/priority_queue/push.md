[sgcl](../../README.md) › [concurrent](../README.md) › [priority_queue](README.md)

# sgcl::concurrent::priority_queue\<T, Compare\>::push

```cpp
void push(const T& value)                                  // (1)
    noexcept(std::is_nothrow_copy_constructible_v<T> &&
             std::is_nothrow_move_constructible_v<T> &&
             std::is_nothrow_move_assignable_v<T>);
void push(T&& value)                                       // (2)
    noexcept(std::is_nothrow_move_constructible_v<T> &&
             std::is_nothrow_move_assignable_v<T>);
```

Inserts an element.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.

Under the lock, the element takes the next number of the queue's counter, is appended to the heap's vector and
sifted up. After the lock, the count of the pushes is raised, and a thread waiting in [pop](pop.md) is woken when
there is one.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to insert |

## Return value

None.

## Complexity

Logarithmic in the size: at most log *n* comparisons, under the lock; amortized constant for the growth of the
heap's vector.

## Exceptions

What the copy constructor (1) of `T`, and the move constructor and the move assignment of `T`, throw; none when
they are noexcept.

When the construction of the element throws, the queue is as it was: the element is not in it, the others keep
their order, `size()` is unchanged and no thread waiting in [pop](pop.md) is woken. A move of `T` that throws while
the element is moved into its place leaves the heap's contents unspecified; the move should not throw.

## Notes

The element is numbered under the lock, so of two equal elements the one whose push took the lock first comes out
first; the pushes of one thread come out in their order. The lock is spun through for the few dozen nanoseconds of
another thread's operation and parked on after that.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct ByPriority {
    bool operator()(const pair<int, string>& a, const pair<int, string>& b) const noexcept {
        return a.first < b.first;
    }
};

int main() {
    concurrent::priority_queue<pair<int, string>, ByPriority> tasks;
    tasks.push({2, "write"});
    tasks.push({1, "read"});
    tasks.push({2, "close"});
    tasks.push({1, "open"});

    while (auto task = tasks.try_pop()) {
        println("{} {}", task->first, task->second);
    }
}
```

Output:

```text
1 read
1 open
2 write
2 close
```

## See also

- [emplace](emplace.md): constructs the element in place
- [try_pop](try_pop.md), [pop](pop.md): take the least element
- [sgcl::concurrent::priority_queue\<T, Compare\>](README.md)
