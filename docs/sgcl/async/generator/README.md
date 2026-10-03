[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::generator\<T\>

```cpp
#include "sgcl/async/generator.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class T>
    class generator;
}
```

`sgcl::async::generator<T>` is a generator that may wait: a coroutine that `co_yield`s values and `co_await`s
between them (a [channel](../channel/README.md), a [sleep](../sleep.md), a [task](../task/README.md)), consumed from a task with
`while (auto v = co_await g.next())`. The generator that does not wait, consumed by a range-for on any thread, is the
core's [generator](../../core/generator/README.md); this one is consumed only by a coroutine, since its values may take a wait
to come. The value type is always spelled, `async::generator<int>`.

The consumer and the generator hand control to each other directly, without the scheduler's queue: `next()` resumes
the generator on the consumer's worker, a `co_yield` resumes the consumer where the generator is, and while the
generator waits for something the consumer waits with it, no thread held by either. The two take turns and never run
at the same time, so the generator makes its next value only when `next()` asks. A producer that is to work ahead of
its consumer is a task of its own with a channel between the two, whose capacity is how far ahead it may get (the
second example).

The generator runs as part of its consumer: at every `next()` its frame takes the consumer's executor and
task-locals, so a wait of its own resumes it where the consumer runs (an [executor](../executor/README.md), a
[strand](../strand/README.md)), and the functions under it, and the consumer resumed by the yield, see the consumer's
[task-locals](../task_local/README.md); a local the generator sets itself lasts until its next yield.

## Rules

- Both frames are on the managed heap: the generator's held by the `generator` object, the consumer's by the
  generator's promise while it waits; the parameters, locals and temporaries of both are roots meanwhile. A value
  yielded is held by the promise until the consumer takes it.
- The generator object is one [frame_ptr](../../core/frame_ptr/README.md): it holds the frame by a root and lives anywhere.
  The consumer is a coroutine with a managed frame, a task; a thread has no form of `next()`.
- The coroutine is lazy: nothing runs until the first `next()`. It is consumed by one coroutine at a time.
- Single pass: every `next()` advances the coroutine, and a `next()` past the end gives nothing again.
- An exception the generator throws comes out of the `next()` that ran into it; the generator is finished after it.
- Move-only: a move hands the frame over and leaves the source empty. Destroying a generator, or a move assignment
  over one, destroys the coroutine at once, wherever it was suspended.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the values: a type `co_yield` moves into the promise, held in an `optional<T>` there and moved out by `next()`. |

## Member types

| Type | Definition |
|---|---|
| `promise_type` | the promise of the coroutine, derived from [managed_frame](../../core/managed_frame.md): `initial_suspend` suspends (the generator is lazy), `yield_value` moves the value in and resumes the consumer, `return_void` and the final suspension resume it too, `unhandled_exception` keeps the exception for `next()`. A program does not call it. |
| `next_op` | the awaiter `next()` returns ([next](next.md)) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](generator.md) | constructs an empty generator, or takes another's over |
| `(destructor)` | destroys the coroutine, if any |
| [operator=](operator_assign.md) | destroys the coroutine held and takes another's over |

#### Observers

| Function | Description |
|---|---|
| [done](done.md) | checks whether the coroutine has ended |

#### Operations

| Function | Description |
|---|---|
| [next](next.md) | runs the coroutine to its next value, waiting with it |

## Complexity

`next()` is a transfer to the generator's coroutine and one back, on the consumer's thread, with no queue of the
scheduler between; a wait of the generator is the wait of what it awaits. The frame is allocated once, as a managed
buffer, when the coroutine function is called.

## Examples

A generator that waits before every value, and hands over values the consumer takes as its own: a
`std::unique_ptr` comes out of `next()` by move.

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <chrono>
#include <memory>

using namespace sgcl;
using namespace std::chrono_literals;

struct Order {
    int id;
    int amount;
};

// Makes the orders one at a time, waiting before each as if for the network
async::generator<std::unique_ptr<Order>> incoming(int count) {
    for (int i : range(1, count + 1)) {
        co_await async::sleep(10ms);  // the consumer waits too, no thread is held
        co_yield std::make_unique<Order>(i, i * 100);
    }
}

async::task<int> process() {
    int total = 0;
    auto orders = incoming(3);
    while (auto order = co_await orders.next()) {
        std::unique_ptr<Order> mine = std::move(*order);
        println("order {}: {}", mine->id, mine->amount);
        total += mine->amount;
    }
    co_return total;
}

int main() {
    println("total {}", async::spawn(process()).wait());
}
```

Output:

```text
order 1: 100
order 2: 200
order 3: 300
total 600
```

A producer that works ahead of its consumer is a task with a channel, not a generator: here the next order is on its
way while the consumer handles the current one. Ten milliseconds to make an order and ten to handle it take about
40 ms for three orders this way; through a generator, which takes turns with its consumer, they take about 60.

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <chrono>
#include <memory>

using namespace sgcl;
using namespace std::chrono_literals;

struct Order {
    int id;
    int amount;
};

// A task beside the consumer: it makes the next order while the current one is handled
async::task<> incoming(async::channel<std::unique_ptr<Order>> out, int count) {
    for (int i : range(1, count + 1)) {
        co_await async::sleep(10ms);
        co_await out.send(std::make_unique<Order>(i, i * 100));
    }
    out.close();
}

async::task<int> process() {
    async::channel<std::unique_ptr<Order>> orders(2);  // up to two orders waiting
    auto producer = async::spawn(incoming(orders, 3));
    int total = 0;
    while (auto order = co_await orders.receive()) {
        co_await async::sleep(10ms);  // the handling of one order
        total += (*order)->amount;
    }
    co_return total;
}

int main() {
    auto start = std::chrono::steady_clock::now();
    int total = async::spawn(process()).wait();
    auto took = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(took);
    println("total {} in {} ms", total, ms.count());
}
```

Sample output:

```text
total 600 in 47 ms
```

## See also

- [generator](../../core/generator/README.md): the core's generator, which does not wait
- [task](../task/README.md): the consumer, a coroutine that produces one value
- [channel](../channel/README.md): between a producer that works ahead and its consumer
- [managed_frame](../../core/managed_frame.md), [frame_ptr](../../core/frame_ptr/README.md): the managed frame under a generator
- [README: Coroutines](../README.md#coroutines)
