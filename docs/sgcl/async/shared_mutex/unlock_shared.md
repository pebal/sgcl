[sgcl](../../README.md) › [async](../README.md) › [shared_mutex](README.md)

# sgcl::async::shared_mutex::unlock_shared

```cpp
void unlock_shared();
```

Gives a reader's lock back: the reader is counted out by an atomic subtract on the word. When a writer waits, the
last of the readers it waits for sends it the signal that lets it in.

## Parameters

None.

## Return value

None.

## Complexity

Constant: one atomic subtract, and, when a writer waits, one more on the count of the readers it waits for.

## Exceptions

`std::system_error` when the signal wakes a waiting writer's task, the wake must start the scheduler's workers and a
thread cannot be started ([README: The rules](../README.md#the-rules), 5).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> writer(tracked_ptr<int> value, async::shared_mutex& m) {
    auto guard = co_await m.scoped_lock();  // waits for the reader
    *value = 2;
}

int main() {
    async::shared_mutex m;
    tracked_ptr value = make_tracked<int>(1);
    m.lock_shared();
    async::task<> w = async::spawn(writer(value, m));
    println("read {}", *value);
    m.unlock_shared();  // the last reader lets the writer in
    w.wait();
    println("read {}", *value);
}
```

Output:

```text
read 1
read 2
```

## See also

- [lock_shared](lock_shared.md), [try_lock_shared](try_lock_shared.md): take the reader's lock
- [unlock](unlock.md): gives the writer's lock back
- [sgcl::async::shared_mutex](README.md)
