[sgcl](../../README.md) › [async](../README.md) › [executor](../executor.md)

# sgcl::async::executor::go

```cpp
/*(1)*/ template<class T>
        void go(task<T> t);
/*(2)*/ template<class F>
        void go(F f);
```

Starts a task on this executor and lets go of it: a [spawn](spawn.md) and a detach. Nobody waits for the task; it
runs to its end on the executor's thread, and what it returns is dropped. An exception it lets out goes to the
handler of [on_unhandled](../on_unhandled.md).

1. Starts `t`. A task is started once: debug builds assert on a task started already.
2. The same for a coroutine function with captures, passed without the call; the closure is moved into a frame of
   the task's own. Takes part only when `F` is called with no arguments and returns a task.

The free form, `async::go(f(), ex)`, is the same ([go](../go.md)).

## Parameters

| Parameter | Description |
|---|---|
| `t` | the task to start; not started before |
| `f` | the coroutine function whose task to start |

## Return value

None.

## Complexity

Constant: a push on the executor's queue, with no allocation; (2) the frame of the task that holds the closure.

## Exceptions

- (1) None.
- (2) What the move of `F` throws.

## Notes

The task of (2) is a task of the library's that holds the closure and awaits the function's task; the function's
task starts when that task first runs, so on an executor it runs one turn later than a task passed to (1).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> greet(string who) {
    println("hello, {}", who);
    co_return;
}

int main() {
    async::executor ex;
    ex.go(greet("Ada"));
    string name = "Grace";
    ex.go([name]() -> async::task<> {
        println("hello, {}", name);
        co_return;
    });
    println("{} run", ex.poll());
    println("{} run", ex.poll());
}
```

Output:

```text
hello, Ada
2 run
hello, Grace
1 run
```

## See also

- [spawn](spawn.md): a task started and kept
- [go](../go.md): the same on the pool of workers, and `go(t, ex)`
- [sgcl::async::executor](../executor.md)
