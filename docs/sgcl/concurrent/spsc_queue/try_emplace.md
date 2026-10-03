[sgcl](../../README.md) › [concurrent](../README.md) › [spsc_queue](../spsc_queue.md)

# sgcl::concurrent::spsc_queue\<T\>::try_emplace

```cpp
template<class... A>
bool try_emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<T, A...>);
```

The producer's. Appends an element constructed in place from `a...` when there is room: `T(std::forward<A>(a)...)`
in the cell at the tail, published as [try_push](try_push.md) publishes it. When the queue is full, nothing is
constructed.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the element is constructed from |

## Return value

`true` when the element was appended, `false` when the queue was full.

## Complexity

Constant.

## Exceptions

What the constructor of `T` throws; none when it is noexcept. The tail moves and the cell is published only once the element is there: the
queue is as it was.

## Notes

Called by the producer alone, one thread at a time, and wait-free, as `try_push` is; `try_push` is this function
with the value as its argument. The blocking [push](push.md) has no emplacing form: construct the element and
push it, or loop on `try_emplace`.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Reading {
    string sensor;
    double value;
};

int main() {
    concurrent::spsc_queue<Reading> readings(2);
    println("{}", readings.try_emplace("left", 20.5));
    println("{}", readings.try_emplace("right", 21.0));
    println("{}", readings.try_emplace("top", 19.5));

    while (auto r = readings.try_pop()) {
        println("{} {}", r->sensor, r->value);
    }
}
```

Output:

```text
true
true
false
left 20.5
right 21
```

## See also

- [try_push](try_push.md): appends a copy or a moved value
- [push](push.md): waits for room
- [sgcl::concurrent::spsc_queue\<T\>](../spsc_queue.md)
