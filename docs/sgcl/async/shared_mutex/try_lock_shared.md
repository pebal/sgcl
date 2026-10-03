[sgcl](../../README.md) › [async](../README.md) › [shared_mutex](README.md)

# sgcl::async::shared_mutex::try_lock_shared

```cpp
bool try_lock_shared() noexcept;
```

Locks the mutex for a reader when no writer holds it or waits for it, and returns at once either way: a
compare-exchange that counts the reader in while the writer's bit is clear. A writer that waits makes it fail, as it
holds back every reader that comes after it.

## Parameters

None.

## Return value

`true` when the reader's lock was taken, `false` when a writer holds the mutex or waits for it.

## Complexity

Constant: a compare-exchange on the word, retried while other readers change the count.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::shared_mutex m;
    println("{} {}", m.try_lock_shared(), m.try_lock_shared());  // two readers at once
    m.unlock_shared();
    m.unlock_shared();
    m.lock();
    println("{}", m.try_lock_shared());  // the writer holds it
    m.unlock();
}
```

Output:

```text
true true
false
```

## See also

- [lock_shared](lock_shared.md): the reader's lock that waits
- [try_lock](try_lock.md): the writer's lock without the wait
- [sgcl::async::shared_mutex](README.md)
