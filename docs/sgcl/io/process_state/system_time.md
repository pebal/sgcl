[sgcl](../../README.md) › [io](../README.md) › [process_state](README.md)

# sgcl::io::process_state::system_time

```cpp
std::chrono::microseconds system_time() const noexcept;
```

Returns the processor time the process spent in the kernel, Go's `ProcessState.SystemTime`, as `wait4` reported it.

## Parameters

None.

## Return value

The time in kernel mode.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command cmd("true");
    (void)cmd.run();
    auto total = cmd.state->user_time() + cmd.state->system_time();
    println("{} us of processor time", total.count());
}
```

Sample output:

```text
1412 us of processor time
```

## See also

- [user_time](user_time.md): the time in user mode
- [sgcl::io::process_state](README.md)
