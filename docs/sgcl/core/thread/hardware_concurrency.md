[sgcl](../../README.md) › [core](../README.md) › [thread](README.md)

# sgcl::thread::hardware_concurrency

```cpp
static unsigned hardware_concurrency() noexcept;
```

The number of threads the hardware runs at once, `std::thread::hardware_concurrency()`: the logical cores the
system reports. A hint, not a limit.

## Parameters

None.

## Return value

The number of hardware threads, or `0` when the system does not say.

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
    unsigned cores = thread::hardware_concurrency();
    println("{} hardware threads", cores);
}
```

Sample output:

```text
8 hardware threads
```

## See also

- [config](../config.md): `workers` and `sweep_threads_max`, the defaults derived from it
- [sgcl::thread](README.md)
