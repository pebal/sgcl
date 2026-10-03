[sgcl](../../README.md) › [async](../README.md) › [task](../task.md)

# sgcl::async::task\<T\>::operator=

```cpp
task& operator=(task&& o) noexcept;
```

Lets go of the task held and takes the coroutine of `o` over; `o` is empty after. A self-assignment does
nothing.

The task held is let go of as the destructor lets go of it: one that started (spawned, waited for, awaited or
resumed) is [detached](detach.md) and runs on to its end wherever it waits, nothing it waits for resuming a destroyed
coroutine; one that never started is destroyed with its frame and never runs. The `operator=` of `task<void>` is the
same.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the task whose coroutine is taken over |

## Return value

`*this`.

## Complexity

Constant, plus the destruction of a frame that never started.

## Exceptions

None.

## Notes

A task let go of is not cancelled: what it sends, writes or changes after its wait it still does, and what it throws
goes to [on_unhandled](../on_unhandled.md)'s handler. A task that is to stop early is given a
[stop_token](../stop_token.md) and stops on it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> doubled(async::channel<int> in, async::channel<int> out) {
    auto v = co_await in.receive();
    co_await out.send(*v * 2);
}

int main() {
    async::channel<int> in, out;
    auto t = async::spawn(doubled(in, out));  // waits in receive()
    t = async::task<>();  // let go of: it runs on
    (void)in.send(21).wait();
    println("{}", *out.receive().wait());

    t = doubled(in, out);
    t = async::task<>();  // never started: destroyed, it never runs
    println("{}", in.try_send(1));
}
```

Output:

```text
42
false
```

## See also

- [detach](detach.md): lets go of the task without assigning
- [(constructor)](task.md): the move constructor
- [sgcl::async::task\<T\>](../task.md)
