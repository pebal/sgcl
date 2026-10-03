[sgcl](../../README.md) › [async](../README.md) › [condition_variable](../condition_variable.md)

# sgcl::async::condition_variable::condition_variable

```cpp
/*(1)*/ condition_variable() = default;
/*(2)*/ condition_variable(const condition_variable&) = delete;
```

1. A condition variable with no waiter: its queue of waiters, empty.
2. A condition variable is not copyable, and not movable: it is an object of one place, which tasks reach through
   the object that holds it.

## Parameters

None.

## Complexity

Constant: the queue's first node on the managed heap.

## Exceptions

None: the defaulted constructor is noexcept, as the queue's is.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Shared {
    async::mutex lock;
    async::condition_variable changed;  // a member of a managed object
};

int main() {
    async::condition_variable local;  // on the stack
    tracked_ptr shared = make_tracked<Shared>();
    local.notify_all();  // no waiter: nothing
    shared->changed.notify_one();
    println("{}", std::is_nothrow_default_constructible_v<async::condition_variable>);
    println("{}", std::is_copy_constructible_v<async::condition_variable>);
}
```

Output:

```text
true
false
```

## See also

- [wait](wait.md): the wait for a notify
- [sgcl::async::condition_variable](../condition_variable.md)
