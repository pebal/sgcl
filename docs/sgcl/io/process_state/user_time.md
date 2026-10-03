[sgcl](../../README.md) › [io](../README.md) › [process_state](README.md)

# sgcl::io::process_state::user_time

```cpp
std::chrono::microseconds user_time() const noexcept;
```

Returns the processor time the process spent in user mode, Go's `ProcessState.UserTime`, as `wait4` reported it.

## Parameters

None.

## Return value

The time in user mode.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command busy("sh", "-c", "i=0; while [ $i -lt 20000 ]; do i=$((i+1)); done");
    (void)busy.run();
    const io::process_state& st = *busy.state;
    println("user {} us, system {} us", st.user_time().count(), st.system_time().count());
}
```

Sample output:

```text
user 41220 us, system 2153 us
```

## See also

- [system_time](system_time.md): the time in the kernel
- [sgcl::io::process_state](README.md)
