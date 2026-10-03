[sgcl](../../README.md) › [async](../README.md) › [strand](../strand.md)

# sgcl::async::strand::busy

```cpp
bool busy() const noexcept;
```

Checks whether a task of the strand runs or is queued at this moment. Another thread may queue or finish a task
right after the look, so the answer is a snapshot.

## Parameters

None.

## Return value

`true` while a task of the strand runs or waits in its queue, `false` when the strand is idle.

## Complexity

Constant: a read of the strand's count.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<bool> look(async::strand& s) {
    co_return s.busy();  // this task is the strand's
}

int main() {
    async::strand s;
    println("idle: {}", !s.busy());
    println("from its own task: {}", s.spawn(look(s)).wait());
}
```

Output:

```text
idle: true
from its own task: true
```

## See also

- [spawn](spawn.md): a task queued on the strand
- [sgcl::async::strand](../strand.md)
