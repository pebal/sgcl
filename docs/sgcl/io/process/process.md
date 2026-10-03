[sgcl](../../README.md) › [io](../README.md) › [process](../process.md)

# sgcl::io::process::process

```cpp
process() noexcept = default;
```

Makes an empty handle, which holds no process: what a [command](../command.md)'s `process` is before its start. The
handle of a running child is made by [command::start](../command/start.md); the copy and the assignment are the
implicit ones, which copy the word, and the copies are the same process.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::process none;
    io::command cmd("true");
    println("{} {}", bool(none), bool(cmd.process));
    (void)cmd.run();
    io::process copy = cmd.process;
    println("{} {}", bool(copy), copy == cmd.process);
}
```

Output:

```text
false false
true true
```

## See also

- [operator bool](operator_bool.md): whether the handle holds a process
- [command::start](../command/start.md): makes the process
- [sgcl::io::process](../process.md)
