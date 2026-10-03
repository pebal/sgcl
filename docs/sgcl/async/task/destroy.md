[sgcl](../../README.md) › [async](../README.md) › [task](README.md)

# sgcl::async::task\<T\>::destroy

```cpp
void destroy() noexcept;
```

Destroys the coroutine now, running the destructors of its locals and promise wherever it is suspended, and leaves
the task empty and [done](done.md); the frame's memory goes to the collector. For a task that never ran, one driven
by hand and suspended by its own `co_await std::suspend_always{}`, or one that is done; never for one that is
queued, running or waiting for something, whose waker would resume a destroyed coroutine: [detach](detach.md) that
one instead. The destructor destroys a task that never started the same way.

## Parameters

None.

## Return value

None.

## Complexity

The destructors of the coroutine's locals and promise.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <coroutine>

using namespace sgcl;

struct Noisy {
    ~Noisy() {
        println("the local destroyed");
    }
};

async::task<int> count_to(int n) {
    Noisy local;
    for (int i : range(n)) {
        co_await std::suspend_always{};
    }
    co_return n;
}

int main() {
    async::task<int> t = count_to(100);
    t.resume();
    t.destroy();  // the other 99 steps never happen
    println("{}", t.done());
}
```

Output:

```text
the local destroyed
true
```

## See also

- [detach](detach.md): lets go of a task that runs or waits
- [frame_ptr::destroy](../../core/frame_ptr/destroy.md): what it calls
- [sgcl::async::task\<T\>](README.md)
