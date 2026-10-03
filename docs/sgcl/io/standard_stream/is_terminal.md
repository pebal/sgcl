[sgcl](../../README.md) › [io](../README.md) › [standard_stream](README.md)

# sgcl::io::standard_stream::is_terminal

```cpp
bool is_terminal() const noexcept;
```

Checks whether the stream's descriptor is a terminal, `isatty`: the question a program asks before it colors its
output. A stream redirected to a file or a pipe is not one.

## Parameters

None.

## Return value

`true` when the descriptor is a terminal.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(io::stdout.is_terminal() ? "\033[1mready\033[0m" : "ready");
}
```

Sample output:

```text
ready
```

## See also

- [io::is_terminal](../is_terminal.md): the same question of any descriptor
- [fd](fd.md): the descriptor
- [sgcl::io::standard_stream](README.md)
