[sgcl](../../README.md) › [core](../README.md) › [thread](../thread.md)

# sgcl::thread::swap, sgcl::swap (sgcl::thread)

```cpp
void swap(thread& o) noexcept;               // (1)
void swap(thread& a, thread& b) noexcept;    // (2), in namespace sgcl
```

1. Swaps the threads this object and `o` stand for.
2. `a.swap(b)`.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the thread object to swap with |
| `a`, `b` | the thread objects to swap |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    thread worker([] {});
    thread none;
    thread::id id = worker.get_id();

    swap(worker, none);
    println("{} {}", worker.joinable(), none.get_id() == id);

    none.swap(worker);
    println("{}", worker.get_id() == id);
    worker.join();
}
```

Output:

```text
false true
true
```

## See also

- [operator=](operator_assign.md): moves a thread object
- [sgcl::thread](../thread.md)
