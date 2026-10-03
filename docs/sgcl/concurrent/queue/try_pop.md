[sgcl](../../README.md) › [concurrent](../README.md) › [queue](../queue.md)

# sgcl::concurrent::queue\<T\>::try_pop

```cpp
optional<T> try_pop() noexcept(std::is_nothrow_move_constructible_v<T>);
```

Takes the first element, or nothing when the queue is empty. The pop walks from the head to the first node whose
element is not taken, claims the element with a compare-exchange on the node's flag, moves it out into the
`optional` returned and destroys it in the node; it swings the head when the element was a node or more past it.
When the walk reaches the last node without finding an element, the queue was empty at that moment.

## Parameters

None.

## Return value

The first element, moved out, or `nullopt` when the queue was empty. `optional` is the alias of `std::optional`
([aliases](../../core/aliases.md)).

## Complexity

Constant, plus the walk over the taken nodes the head has not passed yet: a node or two, more when other threads
pop at once.

## Exceptions

What the move constructor of `T` throws; none when it is noexcept. The element is claimed before it is moved: when its move throws, it is
lost, left in its node for the collector, and the queue is otherwise intact.

## Notes

Lock-free, and linearizable at the compare-exchange on the node's flag: of two threads reaching the same element,
exactly one takes it, and the other walks on to the next. The element is destroyed on the thread that pops it, as
`std::queue::pop` destroys it.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::queue<string> words;
    words.push("first");
    words.push("second");

    while (auto w = words.try_pop()) {
        println("{}", *w);
    }
    println("{}", words.try_pop());
}
```

Output:

```text
first
second
nullopt
```

## See also

- [pop](pop.md): waits for an element
- [push](push.md): appends an element
- [sgcl::concurrent::queue\<T\>](../queue.md)
