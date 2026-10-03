[sgcl](../../README.md) › [async](../README.md) › [broadcast](../broadcast.md)

# sgcl::async::broadcast\<T\>::broadcast

```cpp
explicit broadcast(size_type capacity);    // (1)
broadcast(const broadcast&) = delete;      // (2)
```

1. Makes a broadcast with a ring of `capacity` values, rounded up to a power of two; a `capacity` of 0 is a ring of
   one. The ring and its words are one state on the managed heap, shared with the subscriptions.
2. A broadcast is not copyable, and so not movable either: the subscriptions are of this one.

## Parameters

| Parameter | Description |
|---|---|
| `capacity` | the number of values the ring holds, rounded up to a power of two |

## Complexity

Linear in the capacity: the slots of the ring are made.

## Exceptions

`length_error` when the ring of `capacity` values would be larger than the largest array.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::broadcast<int> events(5);
    async::broadcast<int> one(0);
    println("{} {}", events.capacity(), one.capacity());
}
```

Output:

```text
8 1
```

## See also

- [capacity](capacity.md): the size of the ring
- [subscribe](subscribe.md): the receivers
- [sgcl::async::broadcast\<T\>](../broadcast.md)
