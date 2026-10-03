[sgcl](../../README.md) › [io](../README.md) › [standard_stream](README.md)

# sgcl::io::standard_stream::fd

```cpp
int fd() const noexcept;
```

Returns the descriptor of the stream: 0 for `io::stdin`, 1 for `io::stdout`, 2 for `io::stderr`. Nothing is made:
the stream's file is not.

## Parameters

None.

## Return value

The descriptor.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", io::stderr.fd());
}
```

Output:

```text
2
```

## See also

- [file](file.md): the file over the descriptor
- [is_terminal](is_terminal.md): whether the descriptor is a terminal
- [sgcl::io::standard_stream](README.md)
