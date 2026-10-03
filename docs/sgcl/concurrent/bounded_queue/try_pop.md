[sgcl](../../README.md) › [concurrent](../README.md) › [bounded_queue](../bounded_queue.md)

# sgcl::concurrent::bounded_queue\<T\>::try_pop

```cpp
optional<T> try_pop() noexcept(std::is_nothrow_move_constructible_v<T>);
```

Takes the first element, or nothing when the queue is empty. The pop looks at the cell at the dequeue position:
when its sequence is the position plus one, the cell is published, and the pop wins the position with a
compare-exchange, moves the element out into the `optional` returned, destroys it in the cell and releases the
cell for the next lap, the sequence stored as the position plus the number of cells. A cell published without an
element, by a push whose constructor threw, is released and passed over, and the pop goes on at the next position.
When the cell at the position is not published, the queue was empty at that moment.

## Parameters

None.

## Return value

The first element, moved out, or `nullopt` when the queue was empty. `optional` is the alias of `std::optional`
([aliases](../../core/aliases.md)).

## Complexity

Constant, plus the retries of a lost compare-exchange when other consumers pop at once.

## Exceptions

What the move constructor of `T` throws; none when it is noexcept. The position is won before the element is moved: when its move throws,
the element is destroyed in the cell and lost, the cell released, and the queue is otherwise intact.

## Notes

Lock-free, and linearizable at the compare-exchange on the dequeue position: of two consumers reaching the same
cell, exactly one takes its element, and the other goes on at the next position. A lost exchange, or a position
found stale, backs off as [try_push](try_push.md) does. The element is destroyed on the thread that pops it, as
`std::queue::pop` destroys it; after releasing the cell, the pop wakes the producers waiting in [push](push.md)
on it, when a thread waits at all.

A cell whose position a producer has won and whose element it is still constructing is not published: a
`try_pop` that reaches it returns `nullopt`, even when a later producer's element is already in the next cell,
and the elements come out in the order of their positions.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::bounded_queue<string> words(4);
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
- [try_push](try_push.md), [push](push.md): append an element
- [sgcl::concurrent::bounded_queue\<T\>](../bounded_queue.md)
