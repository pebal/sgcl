[sgcl](../../README.md) › [async](../README.md) › [task_local](../task_local.md)

# sgcl::async::task_local\<T\>::with

```cpp
template<class U>
task<U> with(T value, task<U> t);
```

Returns a task that runs `t` with the value set: a wrapper that [sets](set.md) `value` for itself, awaits `t`, which
inherits it, and returns what `t` returned. The task that awaits or spawns the wrapper keeps its own value; what `t`
or its children set is theirs. `co_await key.with(v, f())` runs `f` as with a value of its own;
`async::spawn(key.with(v, f()))` starts it so.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value `t` runs with |
| `t` | the task to run; not started before, so that it inherits the value at its start |

## Return value

The wrapper, a task not started yet: what `t` returns, or what it throws, when it is awaited.

## Complexity

Constant: the wrapper's frame and the node of the value.

## Exceptions

- The call: what the move constructor of `T` throws, as `value` is moved into the wrapper's frame.
- The wrapper, carried out: what `t` throws, rethrown, and what the move of `T` into the node throws.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task_local<string> role;

async::task<string> whoami() {
    co_return role.get_or("nobody");
}

async::task<> request() {
    co_await role.set("user");
    println("{}", co_await whoami());
    println("{}", co_await role.with("admin", whoami()));
    println("{}", co_await whoami());
}

int main() {
    async::spawn(request()).wait();
}
```

Output:

```text
user
admin
user
```

## See also

- [set](set.md): the value set for the task itself
- [task](../task.md): the wrapper is one
- [sgcl::async::task_local\<T\>](../task_local.md)
