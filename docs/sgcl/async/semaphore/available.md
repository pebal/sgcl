[sgcl](../../README.md) › [async](../README.md) › [semaphore](../semaphore.md)

# sgcl::async::semaphore::available

```cpp
size_t available() const noexcept;
```

Returns the permits free now: the signals in the channel's ring. Other threads and tasks may take or give back
permits at the same moment, so the count is a look, not a promise: an [acquire](acquire.md) after it may still
wait, and a [try_acquire](try_acquire.md) may still fail.

## Parameters

None.

## Return value

The number of permits free at the moment of the look, from zero to the semaphore's maximum.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::semaphore slots(3);
    println("{}", slots.available());
    (void)slots.try_acquire();
    (void)slots.try_acquire();
    println("{}", slots.available());
}
```

Output:

```text
3
1
```

## See also

- [try_acquire](try_acquire.md): takes a permit when one is free
- [sgcl::async::semaphore](../semaphore.md)
