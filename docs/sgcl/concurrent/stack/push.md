[sgcl](../../README.md) › [concurrent](../README.md) › [stack](README.md)

# sgcl::concurrent::stack\<T\>::push

```cpp
void push(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);    // (1)
void push(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>);         // (2)
```

Puts an element on the top of the stack.

1. Puts a copy of `value`.
2. Puts `value`, moved.

The element is constructed in a new node on the managed heap; the push links the node above the head it loaded
and publishes it with a compare-exchange on the head, retrying against the pushes and pops of other threads.
Then, when a thread waits in [pop](pop.md), the push wakes one.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to put |

## Return value

None.

## Complexity

Constant, plus the retries of a lost compare-exchange when other threads push or pop at once.

## Exceptions

What the copy or the move constructor of `T` throws; none when it is noexcept.

If an exception is thrown, nothing is linked and the stack is as it was.

## Notes

Lock-free, and linearizable at the compare-exchange on the head: a push that returned is seen by every
[try_pop](try_pop.md) that starts after it. A lost exchange backs off before the retry, exponentially up to
`config::backoff_max` pause instructions ([config](../../core/config.md)): sixteen threads at the head cost 10.8
ns per operation with it, 607 without ([Benchmarks: Lock-free stack](../benchmarks.md#lock-free-stack)).

A push wakes a thread in `pop` only when one waits: `pop` counts itself before it waits, and a push that finds
the count zero notifies nothing, so a stack nobody waits on pays no wake.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::stack<string> names;
    string first = "Ada";
    names.push(first);
    names.push("Grace");
    println("{}", *names.try_pop());
    println("{}", *names.try_pop());

    concurrent::stack<int> numbers;
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
Grace
Ada
400 elements
```

## See also

- [emplace](emplace.md): constructs the element in place
- [try_pop](try_pop.md), [pop](pop.md): take the top element
- [sgcl::concurrent::stack\<T\>](README.md)
