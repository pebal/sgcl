[sgcl](../../README.md) › [async](../README.md) › [executor](../executor.md)

# sgcl::async::executor::running

```cpp
bool running() const noexcept;
```

Checks whether a thread runs the executor at this moment: a [run](run.md), a [run_until](run_until.md) or a
[poll](poll.md) in progress.

## Parameters

None.

## Return value

`true` while a run or a poll is in progress, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<bool> look(async::executor& ex) {
    co_return ex.running();
}

int main() {
    async::executor ex;
    println("before: {}", ex.running());
    println("from a task on it: {}", ex.run(look(ex)));
    println("after: {}", ex.running());
}
```

Output:

```text
before: false
from a task on it: true
after: false
```

## See also

- [run](run.md), [poll](poll.md): what makes it true
- [sgcl::async::executor](../executor.md)
