[sgcl](../../README.md) › [concurrent](../README.md) › [queue](README.md)

# sgcl::concurrent::queue\<T\>::push

```cpp
void push(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);    // (1)
void push(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>);         // (2)
```

Appends an element at the end of the queue.

1. Appends a copy of `value`.
2. Appends `value`, moved.

The element is constructed in a new node on the managed heap; the push walks from the tail to the last node and
links the new one after it with a compare-exchange on its link, and swings the tail when the walk went two nodes
or more. Then, when a thread waits in [pop](pop.md), the push wakes it.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to append |

## Return value

None.

## Complexity

Constant, plus the walk from the tail to the last node: a node or two, more when other threads push at once.

## Exceptions

What the copy or the move constructor of `T` throws; none when it is noexcept.

If an exception is thrown, nothing is linked and the queue is as it was.

## Notes

Lock-free, and linearizable at the compare-exchange that links the node: a push that returned is seen by every
`try_pop` that starts after it. A push wakes the threads in `pop` only when one waits: `pop` counts itself before
its last look, and a push that finds the count zero notifies nothing, so a queue nobody waits on pays no wake.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::queue<string> names;
    string first = "Ada";
    names.push(first);
    names.push("Grace");
    println("{}", *names.try_pop());
    println("{}", *names.try_pop());

    concurrent::queue<int> numbers;
    vector<thread> producers;
    for (int p : range(4)) {
        producers.emplace_back([&numbers, p] {
            for (int i : range(100)) {
                numbers.push(p * 100 + i);
            }
        });
    }
    for (auto& t : producers) {
        t.join();
    }
    println("{} elements", numbers.size());
}
```

Output:

```text
Ada
Grace
400 elements
```

## See also

- [emplace](emplace.md): constructs the element in place
- [push_range](push_range.md): appends the elements of a range with one exchange
- [try_pop](try_pop.md), [pop](pop.md): take the first element
- [sgcl::concurrent::queue\<T\>](README.md)
