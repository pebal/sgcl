[sgcl](../../README.md) › [async](../README.md) › [shared_mutex](README.md)

# sgcl::async::shared_mutex::try_lock

```cpp
bool try_lock();
```

Locks the mutex for the writer when nobody holds it, and returns at once either way: the writers' mutex taken
without waiting, then the writer's bit set when no reader holds the lock. When a reader holds it, the writers'
mutex is given back and nothing is changed: no reader is held back by a try that failed.

## Parameters

None.

## Return value

`true` when the writer's lock was taken, `false` when another writer or a reader holds the mutex.

## Complexity

Constant.

## Exceptions

`std::system_error` when the writers' mutex, given back after a try that failed, wakes a waiting writer's task, the
wake must start the scheduler's workers and a thread cannot be started
([README: The rules](../README.md#the-rules), 5).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::shared_mutex m;
    m.lock_shared();
    println("{}", m.try_lock());  // a reader holds it
    m.unlock_shared();
    println("{}", m.try_lock());
    println("{}", m.try_lock());  // the writer holds it
    m.unlock();
}
```

Output:

```text
false
true
false
```

## See also

- [lock](lock.md): the writer's lock that waits
- [try_lock_shared](try_lock_shared.md): a reader's lock without the wait
- [sgcl::async::shared_mutex](README.md)
