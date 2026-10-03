[sgcl](../../README.md) › [concurrent](../README.md) › [bounded_queue](../bounded_queue.md)

# sgcl::concurrent::bounded_queue\<T\>::try_push

```cpp
/*(1)*/ bool try_push(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);
/*(2)*/ bool try_push(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>);
```

Appends an element at the end of the queue when there is room, and returns at once when there is none.

1. Appends a copy of `value`.
2. Appends `value`, moved.

The push looks at the cell at the enqueue position: when its sequence is the position, the cell is free, and the
push wins the position with a compare-exchange, constructs the element in the cell and publishes it, the sequence
stored as the position plus one. When the cell still holds the element of the previous lap, the queue is full at
that moment and nothing is constructed.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to append |

## Return value

`true` when the element was appended, `false` when the queue was full. On `false`, (2) has not moved from `value`.

## Complexity

Constant, plus the retries of a lost compare-exchange when other producers push at once.

## Exceptions

What the copy or the move constructor of `T` throws; none when it is noexcept. The position is won before the element is constructed, so the cell is
published without an element, which the consumer that reaches it releases and passes over; the queue is as it
was, except that the cell counts in [size](size.md) until a consumer has passed it.

## Notes

Lock-free, and linearizable at the compare-exchange on the enqueue position. A lost exchange, or a position found
stale because another producer won it, backs off before the retry, exponentially up to 1024 pause instructions:
at eight producers and eight consumers that turns the storm of lost exchanges into near-serial ones
([Benchmarks: The rings against the unbounded queue](../benchmarks.md#the-rings-against-the-unbounded-queue)).

The queue is full for a push when the cell at its position is not yet released: a consumer that has won that cell
and is still moving its element out counts, so `try_push` may return `false` an instant after [size](size.md)
has dropped below the capacity. After publishing, the push wakes the threads waiting in [pop](pop.md) on that
cell, only when a thread waits at all: a queue nobody waits on pays no wake.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::bounded_queue<string> names(2);
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
- [try_pop](try_pop.md), [pop](pop.md): take the first element
- [sgcl::concurrent::bounded_queue\<T\>](../bounded_queue.md)
