[sgcl](../../README.md) › [concurrent](../README.md) › [spsc_queue](../spsc_queue.md)

# sgcl::concurrent::spsc_queue\<T\>::try_pop

```cpp
optional<T> try_pop() noexcept(std::is_nothrow_move_constructible_v<T>);
```

The consumer's. Takes the first element, or nothing when the queue is empty. The pop looks at the sequence of the
cell at the head: when it is the position with the published bit, the element is in; the pop moves it out into
the `optional` returned, destroys it in the cell, moves the head past it and frees the cell for the next lap.
When the cell is not published, the queue is empty.

## Parameters

None.

## Return value

The first element, moved out, or `nullopt` when the queue was empty. `optional` is the alias of `std::optional`
([aliases](../../core/aliases.md)).

## Complexity

Constant.

## Exceptions

What the move constructor of `T` throws; none when it is noexcept. The element is moved before anything else changes: when its move
throws, the element stays in its cell and the head where it was, and the next `try_pop` takes it again.

## Notes

Called by the consumer alone, one thread at a time; a second consumer is a data race on the head. Wait-free: a
load of the head, the consumer's own word, a load of the cell's sequence and two stores, no compare-exchange.
The pop reads no word of the producer's: the cell's sequence says whether the element is in. The element is
destroyed on the consumer's thread, as `std::queue::pop` destroys it; after freeing the cell, the pop wakes the
producer when it waits in [push](push.md) on that cell, and only then.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::spsc_queue<string> words(4);
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
- [try_push](try_push.md), [push](push.md): the producer's side
- [sgcl::concurrent::spsc_queue\<T\>](../spsc_queue.md)
