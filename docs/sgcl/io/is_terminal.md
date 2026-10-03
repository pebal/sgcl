[sgcl](../README.md) › [io](README.md)

# sgcl::io::is_terminal

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    bool is_terminal(int fd) noexcept;
}
```

Checks whether the descriptor `fd` is a terminal, the C library's `isatty`. A program decides by it whether to
color its output or ask a question: a standard stream redirected to a file or a pipe is not a terminal. The
standard streams ask it of themselves, [standard_stream::is_terminal](standard_stream/is_terminal.md).

## Parameters

| Parameter | Description |
|---|---|
| `fd` | the descriptor |

## Return value

`true` when the descriptor is open and is a terminal.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    bool in = io::is_terminal(0);
    bool out = io::is_terminal(1);
    bool err = io::is_terminal(2);
    println("stdin {}, stdout {}, stderr {}", in, out, err);
    println("{}", io::is_terminal(-1));
}
```

Sample output:

```text
stdin true, stdout false, stderr true
false
```

## See also

- [standard_stream](standard_stream/README.md): `io::stdin`, `io::stdout`, `io::stderr`
- [file::fd](file/fd.md): the descriptor of a file
