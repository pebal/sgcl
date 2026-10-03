[sgcl](../../README.md) › [async](../README.md) › [semaphore](README.md)

# sgcl::async::semaphore::try_acquire

```cpp
bool try_acquire() noexcept;
```

Takes a permit when one is free, and returns at once either way: a receive of the channel's signal that does not
wait. A task may call it, since it never waits.

## Parameters

None.

## Return value

`true` when a permit was taken, `false` when none was free.

## Complexity

Constant.

## Exceptions

None. A receive of a channel may wake a waiting sender, whose wake may start the scheduler's workers, but a
semaphore's channel never has one: [release](release.md) never waits.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::semaphore slots(2);
    println("{}", slots.try_acquire());
    println("{}", slots.try_acquire());
    println("{}", slots.try_acquire());  // both taken
    slots.release();
    println("{}", slots.try_acquire());
}
```

Output:

```text
true
true
false
true
```

## See also

- [acquire](acquire.md): the permit that waits
- [available](available.md): the permits free now
- [sgcl::async::semaphore](README.md)
