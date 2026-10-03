[sgcl](../../README.md) › [io](../README.md) › [process](README.md)

# sgcl::io::process::pid

```cpp
int pid() const noexcept;
```

Returns the id of the process, as `posix_spawn` gave it. The id stays the handle's after the wait, when the system may
have given it to another process.

## Parameters

None.

## Return value

The process id.

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
    println("child {}, ended as {}", cmd.process.pid(), cmd.state->pid());
}
```

Sample output:

```text
child 51870, ended as 51870
```

## See also

- [process_state::pid](../process_state/pid.md): the id in the state of an ended process
- [io::pid](../pid.md): the id of the program's own process
- [sgcl::io::process](README.md)
