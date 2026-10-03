[sgcl](../../README.md) › [async](../README.md) › [blocking_task](../blocking_task.md)

# sgcl::async::blocking_task\<T\>::operator=

```cpp
/*(1)*/ blocking_task& operator=(blocking_task&& other) noexcept = default;
/*(2)*/ blocking_task& operator=(const blocking_task&) = delete;
```

1. Takes over the job of `other`, which is left empty. The job this handle held before is let go of: it runs on all
   the same, and its result is dropped.
2. A handle is not copyable.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose job to take over |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::blocking_task<int> current = async::spawn_blocking([] { return 1; });
    current = async::spawn_blocking([] { return 2; });  // the first job runs on, unread
    println("{}", current.wait());
}
```

Output:

```text
2
```

## See also

- [(constructor)](blocking_task.md): an empty handle, or one moved
- [sgcl::async::blocking_task\<T\>](../blocking_task.md)
