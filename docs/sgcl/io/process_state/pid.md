[sgcl](../../README.md) › [io](../README.md) › [process_state](../process_state.md)

# sgcl::io::process_state::pid

```cpp
int pid() const noexcept;
```

Returns the id the process had. The system may have given the id to another process since the wait.

## Parameters

None.

## Return value

The process id; 0 for a default-constructed state.

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
    println("{}", cmd.state->pid() == cmd.process.pid());
}
```

Output:

```text
true
```

## See also

- [process::pid](../process/pid.md): the id of the running process
- [sgcl::io::process_state](../process_state.md)
