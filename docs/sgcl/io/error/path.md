[sgcl](../../README.md) › [io](../README.md) › [error](README.md)

# sgcl::io::error::path

```cpp
const string& path() const noexcept;
```

Returns what the operation was on: the path of a file, as the program gave it, or the name of a stream
(`"buffer"`); for a child process, its program. An error of [flags](../flags/README.md) carries Go's message about the
command line here.

## Parameters

None.

## Return value

The path or the name; the empty string when the operation was on none.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", io::open("logs/missing.txt").error().path());
    println("{}", io::buffer().seek(-1).error().path());
}
```

Output:

```text
logs/missing.txt
buffer
```

## See also

- [op](op.md): the operation
- [message](message.md): the text of the error
- [sgcl::io::error](README.md)
