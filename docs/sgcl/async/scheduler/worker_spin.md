[sgcl](../../README.md) › [async](../README.md) › [scheduler](README.md)

# sgcl::async::scheduler::worker_spin

```cpp
static duration worker_spin() noexcept;
```

Returns how long a worker with nothing to run looks for work before it sleeps in the kernel: the time
[set_worker_spin](set_worker_spin.md) set, else `SGCL_WORKER_SPIN_US` from the environment once the scheduler has
started (it is read at the first start), else `config::worker_spin_microseconds`.

## Parameters

None.

## Return value

The spin time, in whole microseconds.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    println("{}", async::scheduler::worker_spin());
    async::scheduler::set_worker_spin(1ms);
    println("{}", async::scheduler::worker_spin());
}
```

Output:

```text
20µs
1ms
```

## See also

- [set_worker_spin](set_worker_spin.md): sets it
- [sgcl::async::scheduler](README.md)
