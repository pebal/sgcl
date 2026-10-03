[sgcl](../../README.md) › [concurrent](../README.md) › [spsc_queue](README.md)

# sgcl::concurrent::spsc_queue\<T\>::try_push

```cpp
bool try_push(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);    // (1)
bool try_push(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>);         // (2)
```

The producer's. Appends an element at the end of the queue when there is room, and returns at once when there is
none.

1. Appends a copy of `value`.
2. Appends `value`, moved.

The push looks at the sequence of the cell at the tail: when it is the position, the consumer has taken the
element of the lap before and the cell is free; the push constructs the element there, moves the tail past it and
publishes the cell. When the cell still holds the element of the lap before, the queue is full and nothing is
constructed.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to append |

## Return value

`true` when the element was appended, `false` when the queue was full. On `false`, (2) has not moved from `value`.

## Complexity

Constant.

## Exceptions

What the copy or the move constructor of `T` throws; none when it is noexcept. The tail moves and the cell is published only once the element is
there: the queue is as it was.

## Notes

Called by the producer alone, one thread at a time; a second producer is a data race on the tail. Wait-free: a
load of the tail, the producer's own word, a load of the cell's sequence and two stores, no compare-exchange.
The push reads no word of the consumer's: the cell's sequence says whether it is free. After publishing, it wakes
the consumer when the consumer waits in [pop](pop.md) on that cell, and only then.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::spsc_queue<string> names(2);
    string first = "Ada";
    println("{}", names.try_push(first));
    println("{}", names.try_push("Grace"));
    println("{}", names.try_push("Linus"));  // the ring of two is full

    println("{}", *names.try_pop());
    println("{}", names.try_push("Linus"));
    println("{}", names.size());
}
```

Output:

```text
true
true
false
Ada
true
2
```

## See also

- [try_emplace](try_emplace.md): constructs the element in place
- [push](push.md): waits for room
- [try_pop](try_pop.md), [pop](pop.md): the consumer's side
- [sgcl::concurrent::spsc_queue\<T\>](README.md)
