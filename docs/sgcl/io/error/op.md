[sgcl](../../README.md) › [io](../README.md) › [error](../error.md)

# sgcl::io::error::op

```cpp
const string& op() const noexcept;
```

Returns the operation that failed, as the module names it: `"open"`, `"read"`, `"write"`, `"mkdir"`, `"seek"`,
`"start"`, `"wait"`; or the name a stream or a function of the program's gave.

## Parameters

None.

## Return value

The name of the operation; the empty string for a default-constructed error.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", io::open("missing.txt").error().op());
    println("{}", io::buffer().seek(-1).error().op());
    println("{}", io::mkdir(".").error().op());
}
```

Output:

```text
open
seek
mkdir
```

## See also

- [path](path.md): what the operation was on
- [message](message.md): the text of the error
- [sgcl::io::error](../error.md)
