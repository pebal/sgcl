[sgcl](../../README.md) › [async](../README.md) › [task_local](README.md)

# sgcl::async::task_local\<T\>::set

```cpp
setter set(T value) noexcept(std::is_nothrow_move_constructible_v<T>);
```

Returns an awaitable that sets the value: `co_await key.set(v)` makes `v` the value of the task that awaits it, from
the next line on, and of every task it starts from then on. The awaitable never suspends: the `co_await` is what
names the coroutine whose value it is, so a function the task calls reads the value, and only the task sets it.

The value goes into a new node put in front of the task's chain. The tasks the task started before keep the chain
they took, and its parent and siblings never see it. A task that sets the same key again reads the newest value.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value; moved into the awaitable, and from there into the node |

## Return value

A `setter`, the awaitable. `co_await` gives nothing; there is no form for a thread, which has no value of its own.

## Complexity

Constant: a node made and linked when the awaitable is awaited.

## Exceptions

- The call: what the move constructor of `T` throws; none when it is noexcept.
- The `co_await`: the same, for the move into the node.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task_local<string> stage;

async::task<> child() {
    println("child sees {}", stage.get_or("nothing"));
    co_await stage.set("child");
    println("child set {}", stage.get_or("nothing"));
}

async::task<> parent() {
    co_await stage.set("parent");
    co_await child();
    println("parent keeps {}", stage.get_or("nothing"));
    co_await stage.set("parent, again");
    println("parent now {}", stage.get_or("nothing"));
}

int main() {
    async::spawn(parent()).wait();
}
```

Output:

```text
child sees parent
child set child
parent keeps parent
parent now parent, again
```

## See also

- [with](with.md): a task run with a value, the caller's left as it was
- [get](get.md), [get_or](get_or.md): the value read
- [sgcl::async::task_local\<T\>](README.md)
