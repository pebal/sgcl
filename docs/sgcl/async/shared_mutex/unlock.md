[sgcl](../../README.md) › [async](../README.md) › [shared_mutex](../shared_mutex.md)

# sgcl::async::shared_mutex::unlock

```cpp
void unlock();
```

Gives the writer's lock back: the writer's bit is cleared, the readers it held back are woken by the close of the
channel they wait on, and the writers' mutex is unlocked, letting the next writer in. The readers held back go in
before that next writer: they are counted in the word, and the next writer waits for them.

## Parameters

None.

## Return value

None.

## Complexity

Constant: an atomic subtract, the close of the round's channel when readers wait, and the unlock of the writers'
mutex, each waking what waits.

## Exceptions

`std::system_error` when the close or the unlock wakes a waiting task, the wake must start the scheduler's workers
and a thread cannot be started ([README: The rules](../README.md#the-rules), 5).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> reader(tracked_ptr<int> value, async::shared_mutex& m) {
    auto guard = co_await m.scoped_lock_shared();  // waits for the writer
    co_return *value;
}

int main() {
    async::shared_mutex m;
    tracked_ptr value = make_tracked<int>(0);
    m.lock();
    async::task<int> a = async::spawn(reader(value, m));
    async::task<int> b = async::spawn(reader(value, m));
    *value = 7;
    m.unlock();  // both readers go in
    println("{} {}", a.wait(), b.wait());
}
```

Output:

```text
7 7
```

## See also

- [lock](lock.md), [try_lock](try_lock.md): take the writer's lock
- [unlock_shared](unlock_shared.md): gives a reader's lock back
- [sgcl::async::shared_mutex](../shared_mutex.md)
