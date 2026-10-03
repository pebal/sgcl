[sgcl](../../README.md) › [io](../README.md) › [process](README.md)

# sgcl::io::process::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a process: a [command](../command/README.md)'s `process` does after its start.

## Parameters

None.

## Return value

`true` when the handle holds a process, `false` for an empty one.

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
    println("{}", bool(cmd.process));
    (void)cmd.start();
    println("{}", bool(cmd.process));
    (void)cmd.wait();
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](process.md): an empty handle
- [sgcl::io::process](README.md)
