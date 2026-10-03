[sgcl](../../README.md) › [async](../README.md) › [task_local](../task_local.md)

# sgcl::async::task_local\<T\>::is_set

```cpp
bool is_set() const noexcept;
```

Checks whether the task the calling thread runs has a value for this key, set by itself or inherited, without copying
the value out.

## Parameters

None.

## Return value

`true` when [get](get.md) would give a value, `false` when it would give `nullopt`.

## Complexity

Linear in the number of sets in the task's chain, newest first; constant for a handful of keys.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task_local<int> deadline_ms;

async::task<bool> inherited() {
    co_return deadline_ms.is_set();
}

async::task<> handler() {
    println("before: {}", deadline_ms.is_set());
    co_await deadline_ms.set(250);
    println("after: {}", deadline_ms.is_set());
    println("in a child: {}", co_await inherited());
}

int main() {
    async::spawn(handler()).wait();
    println("outside a task: {}", deadline_ms.is_set());
}
```

Output:

```text
before: false
after: true
in a child: true
outside a task: false
```

## See also

- [get](get.md): the value itself
- [sgcl::async::task_local\<T\>](../task_local.md)
