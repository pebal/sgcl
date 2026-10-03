[sgcl](../../README.md) › [async](../README.md) › [task_local](../task_local.md)

# sgcl::async::task_local\<T\>::task_local

```cpp
/*(1)*/ task_local() noexcept = default;
/*(2)*/ task_local(const task_local&) = delete;
```

1. Constructs the key. The object holds nothing: its address is the key, and the values are in the nodes the tasks'
   frames point to, so it is declared once, at namespace scope, and lives anywhere.
2. A `task_local` is not copyable, nor movable: a copy would be another key.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task_local<int> depth;  // one key, at namespace scope

async::task<> nested(int n) {
    co_await depth.set(n);
    println("depth {}", depth.get_or(-1));
    if (n < 2) {
        co_await nested(n + 1);
    }
    println("back at {}", depth.get_or(-1));
}

int main() {
    async::spawn(nested(0)).wait();
}
```

Output:

```text
depth 0
depth 1
depth 2
back at 2
back at 1
back at 0
```

## See also

- [set](set.md), [get](get.md): the value of a task
- [sgcl::async::task_local\<T\>](../task_local.md)
