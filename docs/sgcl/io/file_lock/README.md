[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::file_lock

```cpp
#include "sgcl/io/lock.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class file_lock;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::io::file_lock` is a lock held on a file, as [lock_file](../lock_file.md) took it, and the guard that gives
it back: [unlock](unlock.md), or the destructor when nothing unlocked it, so that no return and no exception leaves a
file locked, the way `std::unique_lock` holds a mutex. It holds the [file](../file/README.md) itself, a handle, so that
the descriptor, and the lock with it, stays open for as long as the guard does.

## Rules

- Made by [lock_file](../lock_file.md) and [async_lock_file](../lock_file.md); one made by its default constructor
  holds no lock.
- Moved, not copied: one guard unlocks. A guard moved from holds nothing; one assigned to unlocks its own lock first.
- A file closed while the guard holds its lock has given the lock back with its descriptor.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](file_lock.md) | constructs the guard: none, or the one moved from |
| `(destructor)` | unlocks, unless unlocked already |
| [operator=](operator_assign.md) | unlocks its own lock and takes another guard's |

#### Locking

| Function | Description |
|---|---|
| [unlock](unlock.md) | gives the lock back |

#### Observers

| Function | Description |
|---|---|
| [mode](mode.md) | shared or exclusive |
| [offset](offset.md) | the first byte of the range |
| [length](length.md) | the bytes of the range |
| [file](file.md) | the file locked |
| [operator bool](operator_bool.md) | checks whether it holds a lock |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (int i : range(2)) {
        io::file_lock held = io::lock_file("job.lock", {.timeout = duration::zero()}).value();
        println("run {} has the lock", i);
    }  // unlocked at the end of each round
}
```

Output:

```text
run 0 has the lock
run 1 has the lock
```

## See also

- [lock_file](../lock_file.md): what makes one
- [lock_options](../lock_options.md)
