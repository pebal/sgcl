[sgcl](../../README.md) › [async](../README.md) › [task_local](README.md)

# sgcl::async::task_local\<T\>::get

```cpp
optional<T> get() const noexcept(std::is_nothrow_copy_constructible_v<T>);
```

Returns a copy of the value of the task the calling thread runs: the one it set last, or the one it inherited from
the task that started it. It may be called from the coroutine's body or from any function the task calls, however
deep: the thread knows the frame it runs.

## Parameters

None.

## Return value

The value, or `nullopt` when the task never set it and inherited none, and outside a task (a plain thread, a worker
between two tasks).

## Complexity

Linear in the number of sets in the task's chain, newest first; constant for a handful of keys.

## Exceptions

What the copy constructor of `T` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task_local<int> request_id;

void report() {  // a plain function, called by a task
    if (auto id = request_id.get()) {
        println("request {}", *id);
    } else {
        println("no request");
    }
}

async::task<> handle(int id) {
    report();
    co_await request_id.set(id);
    report();
}

int main() {
    async::spawn(handle(7)).wait();
    report();
}
```

Output:

```text
no request
request 7
no request
```

## See also

- [get_or](get_or.md): the value, or a fallback
- [is_set](is_set.md): whether there is a value
- [set](set.md): sets it
- [sgcl::async::task_local\<T\>](README.md)
