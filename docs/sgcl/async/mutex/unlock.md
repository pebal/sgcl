[sgcl](../../README.md) › [async](../README.md) › [mutex](../mutex.md)

# sgcl::async::mutex::unlock

```cpp
void unlock() const;
```

Unlocks the mutex: a send of the signal back into the channel, which hands it to the first waiter when there is one,
a thread in [lock](lock.md), a task in [scoped_lock](scoped_lock.md) or a select with an [on_lock](on_lock.md) case.

Any thread or task may unlock, not only the one that locked: the mutex has no owner, and an unlock from elsewhere is
a hand-over. An unlock of a mutex that is not locked is lost, since the channel holds one signal at most: the next
lock takes the mutex and the one after it waits.

## Parameters

None.

## Return value

None.

## Complexity

Constant: a send into the ring, and a wake when a waiter is there.

## Exceptions

`std::system_error` when the wake of a waiting task must start the scheduler's workers and a thread cannot be
started ([README: The rules](../README.md#the-rules), 5).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> holder(async::mutex m) {
    auto guard = co_await m.scoped_lock();  // waits until main unlocks
    println("the task holds it");
}

int main() {
    async::mutex m;
    m.lock();
    async::task<> t = async::spawn(holder(m));
    println("main unlocks");
    m.unlock();
    t.wait();

    m.unlock();  // not locked: lost
    bool first = m.try_lock();
    bool second = m.try_lock();
    println("{} {}", first, second);
    m.unlock();
}
```

Output:

```text
main unlocks
the task holds it
true false
```

## See also

- [lock](lock.md), [try_lock](try_lock.md), [scoped_lock](scoped_lock.md): lock the mutex
- [mutex::guard](../mutex-guard.md): the unlock at the end of a scope
- [sgcl::async::mutex](../mutex.md)
