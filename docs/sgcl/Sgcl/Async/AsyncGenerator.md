# Sgcl::AsyncGenerator

```cpp
#include "sgcl/Sgcl/Async/AsyncGenerator.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T> class AsyncGenerator;   // a generator that may wait between its values
}
```

The same class in the `sgcl` interface: [async_generator](../../async/coroutine.md#async_generator). The frames it runs on are the ones of [Coroutine](Coroutine.md).

## Rules

- Those of a [Task](Coroutine.md#rules): a coroutine on a managed frame, held by the `AsyncGenerator` object; single pass; move-only.
- The consumer is a task (or any coroutine on a managed frame): `Next()` is awaited, never called from a thread.

## Members

### AsyncGenerator

```cpp
template<class T>
class AsyncGenerator;

auto Next() noexcept;                     // co_await g.Next(): Optional<T>, None at the end; rethrows
bool IsDone() const noexcept;
InnerType& Inner() noexcept;
```

A generator that may wait: a coroutine that `co_yield`s values and `co_await`s between them (a channel, a sleep, a task), consumed from a task with `while (auto v = co_await g.Next())`. The consumer and the generator hand control to each other directly, without the scheduler's queue: `Next()` resumes the generator on the consumer's worker, a `co_yield` resumes the consumer where the generator is, and while the generator waits for something the consumer waits with it, no thread held by either. The generator runs as part of its consumer: at every `Next()` its frame takes the consumer's executor and task-locals, so a wait of its own resumes it where the consumer runs (an [Executor](Executor.md), a strand) and the functions under it, and the consumer resumed by the yield, see the consumer's [task-locals](TaskLocal.md); a local the generator sets itself lasts until its next yield. Both frames are on the managed heap: the generator's held by the `AsyncGenerator` object, the consumer's by the generator's promise while it waits, and a value yielded is held by the promise until the consumer takes it. Move-only; single pass; `Next()` past the end gives `None` again; an exception the generator throws comes out of the `Next()` that ran into it.

```cpp
AsyncGenerator<int> Tens(Channel<int>& in) {
    while (auto v = co_await in.AsyncReceive()) {     // waits between yields
        co_yield *v * 10;
    }
}

Task<int> Consume(Channel<int>& in) {
    AsyncGenerator g = Tens(in);
    int sum = 0;
    while (auto v = co_await g.Next()) {              // None once in is closed and drained
        sum += *v;
    }
    co_return sum;
}
```

## See also

- [Coroutine](Coroutine.md): `Task` and `Generator`, the frames; [Channel](Channel.md): what a generator waits on between its values; [Executor](Executor.md), [TaskLocal](TaskLocal.md): what the generator takes from its consumer
