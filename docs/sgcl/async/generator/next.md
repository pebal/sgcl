[sgcl](../../README.md) › [async](../README.md) › [generator](README.md)

# sgcl::async::generator\<T\>::next

```cpp
next_op next() noexcept;
```

Returns the awaiter of the next value: `co_await g.next()` runs the generator's coroutine to its next `co_yield` and
gives the value, moved out of the promise, or gives nothing when the coroutine has ended. The consumer is a coroutine
with a managed frame, a task; there is no form of `next()` for a thread.

The awaiting coroutine hands its thread to the generator directly, without the scheduler's queue: the generator runs
on the consumer's worker, and its `co_yield` resumes the consumer where the generator is. A wait of the generator
between two values (a channel, a sleep, a task) is the consumer's too: neither holds a thread meanwhile, and the
consumer's frame is held by the generator's promise for the length of the wait. Before it resumes the generator,
`next()` gives its frame the consumer's executor and task-locals, so that a wait of the generator resumes it where the
consumer runs and the functions under it see the consumer's [task-locals](../task_local/README.md).

A `next()` past the end, or on an empty generator, gives nothing at once. An exception the coroutine throws comes out
of the `co_await` that ran into it, once; the generator is finished after it.

## Parameters

None.

## Return value

The awaiter of `co_await g.next()`, which gives an `optional<T>`: the value of the next `co_yield`, or empty when the
coroutine has ended.

## Complexity

A transfer to the generator's coroutine and one back, plus what the coroutine runs and waits for until its next
`co_yield`.

## Exceptions

The call throws nothing; carried out, `co_await g.next()` rethrows what the coroutine threw, and the move of the value
out of the promise throws what the move constructor of `T` throws.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

// Waits between yields: for each value it receives
async::generator<int> tens(async::channel<int> in) {
    while (auto v = co_await in.receive()) {
        if (*v < 0) {
            throw std::invalid_argument("negative");
        }
        co_yield *v * 10;
    }
}

async::task<> consume(async::channel<int> in) {
    auto g = tens(in);
    try {
        while (auto v = co_await g.next()) {
            println("{}", *v);
        }
    } catch (const std::exception& e) {
        println("error: {}", e.what());
    }
    println("{} {}", g.done(), (co_await g.next()).has_value());
}

int main() {
    async::channel<int> in(4);
    for (int v : {1, 2, -3}) {
        (void)in.send(v).wait();
    }
    consume(in).wait();
}
```

Output:

```text
10
20
error: negative
true false
```

## See also

- [done](done.md): checks whether the coroutine has ended
- [receive](../channel/receive.md): what the generator above waits on
- [sgcl::async::generator\<T\>](README.md)
