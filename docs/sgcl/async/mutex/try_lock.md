[sgcl](../../README.md) › [async](../README.md) › [mutex](../mutex.md)

# sgcl::async::mutex::try_lock

```cpp
bool try_lock() const noexcept;
```

Locks the mutex when it is free, and returns at once either way: a receive of the channel's signal that does not
wait. The standard's Lockable member, for `std::unique_lock(m, std::try_to_lock)`; a task may call it too, since it
never waits.

## Parameters

None.

## Return value

`true` when the mutex was free and is now locked by the call, `false` when it was locked.

## Complexity

Constant.

## Exceptions

None. A receive of a channel may wake a waiting sender, whose wake may start the scheduler's workers, but the
mutex's channel never has one: [unlock](unlock.md) never waits.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::mutex m;
    println("{}", m.try_lock());
    println("{}", m.try_lock());
    m.unlock();
    println("{}", m.try_lock());
    m.unlock();
}
```

Output:

```text
true
false
true
```

## See also

- [lock](lock.md): the lock that waits, on a thread
- [scoped_lock](scoped_lock.md): the lock that waits, in a task
- [on_lock](on_lock.md): the lock as a case of a select
- [sgcl::async::mutex](../mutex.md)
