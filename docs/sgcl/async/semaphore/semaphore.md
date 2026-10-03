[sgcl](../../README.md) › [async](../README.md) › [semaphore](../semaphore.md)

# sgcl::async::semaphore::semaphore

```cpp
explicit semaphore(size_t permits, size_t max = 0);    // (1)
semaphore(const semaphore&) = delete;                  // (2)
```

1. A semaphore with `permits` free permits and room for `max`: a channel of capacity `max` with `permits` signals in
   it. A `max` of zero is `permits`, or one when `permits` is zero too: `semaphore(0)` is made closed and opened by
   a release, which a channel of capacity zero would lose with nobody waiting for it. Permits above `max` are
   dropped.
2. A semaphore is not copyable, and not movable: it is an object of one place, which tasks reach through the object
   that holds it.

## Parameters

| Parameter | Description |
|---|---|
| `permits` | the permits free from the start |
| `max` | the most permits the semaphore holds; zero for `permits`, or one when `permits` is zero |

## Complexity

Linear in `permits`: a send each, into the channel's ring of `max` slots or more.

## Exceptions

`length_error` when `max` is above what the channel's ring may hold.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::semaphore four(4);
    async::semaphore some(1, 3);
    async::semaphore closed(0);
    println("{} {} {}", four.available(), some.available(), closed.available());
    some.release();
    some.release();
    some.release();  // past the maximum: lost
    closed.release();
    closed.release();
    println("{} {}", some.available(), closed.available());
}
```

Output:

```text
4 1 0
3 1
```

## See also

- [acquire](acquire.md), [release](release.md): take and give back a permit
- [available](available.md): the permits free now
- [sgcl::async::semaphore](../semaphore.md)
