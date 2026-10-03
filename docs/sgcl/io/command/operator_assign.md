[sgcl](../../README.md) › [io](../README.md) › [command](README.md)

# sgcl::io::command::operator=

```cpp
command& operator=(command&&) noexcept = default;
```

Takes another command over: its program, its arguments, its streams, its process and the tasks that serve its pipes.
A command is moved, not copied: it owns those tasks, as `exec.Cmd` owns its goroutines. A command that was started
and is assigned over before its wait leaves its child unwaited for.

## Parameters

| Parameter | Description |
|---|---|
| (unnamed) | the command to take over |

## Return value

`*this`.

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
    cmd = io::command("echo", "moved");
    if (auto out = cmd.output()) {
        print("{}", *out);
    }
}
```

Output:

```text
moved
```

## See also

- [(constructor)](command.md): the program and its arguments
- [sgcl::io::command](README.md)
