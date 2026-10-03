[sgcl](../../README.md) › [async](../README.md) › [strand](../strand.md)

# sgcl::async::strand::strand

```cpp
strand() noexcept;                 // (1)
strand(const strand&) = delete;    // (2)
```

1. Constructs a strand with an empty queue, a managed object the strand holds through a root. It runs nothing and
   costs no thread until a task is queued on it.
2. A strand is not copyable, nor movable: the tasks on it hold its queue.

## Parameters

None.

## Complexity

Constant: the queue, and its stub frame, made.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> one() {
    co_return 1;
}

int main() {
    async::strand s;
    println("busy: {}", s.busy());
    println("{}", s.spawn(one()).wait());
}
```

Output:

```text
busy: false
1
```

## See also

- [spawn](spawn.md): a task started on the strand
- [sgcl::async::strand](../strand.md)
