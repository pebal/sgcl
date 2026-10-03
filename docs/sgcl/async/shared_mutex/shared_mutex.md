[sgcl](../../README.md) › [async](../README.md) › [shared_mutex](../shared_mutex.md)

# sgcl::async::shared_mutex::shared_mutex

```cpp
shared_mutex() noexcept = default;             // (1)
shared_mutex(const shared_mutex&) = delete;    // (2)
```

1. An unlocked shared mutex: the word at zero, no reader and no writer, and the writers' mutex and the channel of
   the writer's wait made with it.
2. A shared mutex is not copyable, and not movable: it is an object of one place, which tasks reach through the
   object that holds it.

## Parameters

None.

## Complexity

Constant: the two channels inside the object, the writers' mutex and the writer's wait, made with it.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Cache {
    async::shared_mutex lock;  // a member of a managed object
    map<string, int> entries;
};

int main() {
    async::shared_mutex local;  // on the stack
    tracked_ptr cache = make_tracked<Cache>();
    println("{} {}", local.try_lock(), cache->lock.try_lock_shared());
    local.unlock();
    cache->lock.unlock_shared();
    println("{}", std::is_move_constructible_v<async::shared_mutex>);
}
```

Output:

```text
true true
false
```

## See also

- [lock](lock.md), [lock_shared](lock_shared.md): take the lock
- [sgcl::async::shared_mutex](../shared_mutex.md)
