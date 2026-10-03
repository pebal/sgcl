[sgcl](../../README.md) › [core](../README.md) › [collector](../collector.md)

# sgcl::collector::terminate

```cpp
static void terminate() noexcept;
```

Stops the collector: the current cycle finishes, cycles run until nothing dies any more (the objects still
reachable are not destroyed), the helper threads and the collector thread exit, and the call returns.

After it no cycle runs: objects are still allocated and destroyed through `unique_ptr`, tracked garbage stays until
the process exits, and `force_collect(true)` returns `false`.

## Parameters

None.

## Return value

None.

## Complexity

The time of the cycles until nothing dies any more.

## Exceptions

None.

## Notes

Optional: a program may simply end. The main thread's exit stops the collector the same way, before the static
destructors run, and those may still use the library (a global `unique_ptr`'s object making objects in its
destructor), since the main thread's registration is never undone.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    tracked_ptr kept = make_tracked<int>(1);
    collector::terminate();  // the collector's threads are gone from here on

    tracked_ptr later = make_tracked<int>(2);  // allocation goes on
    println("{} {}", *kept + *later, collector::force_collect(true));
}
```

Output:

```text
3 false
```

## See also

- [force_collect](force_collect.md): returns `false` once the collector is terminating
- [sgcl::collector](../collector.md)
