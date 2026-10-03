[sgcl](../../README.md) › [io](../README.md) › [error](../error.md)

# sgcl::io::error::message

```cpp
string message() const noexcept;
```

Returns the error as a text for a person, as Go's `*PathError` prints: the operation, a space and the path when
there is one, `: `, and what the code says, `open log.txt: No such file or directory`. The text of a code of the
system is the platform's (`strerror`); the text of an [errc](../errc.md) is the module's. Without an operation the
text begins with the path, `notes.txt: invalid path`; without both, it is what the code says alone.

## Parameters

None.

## Return value

The text, made at the call.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", io::open("config.toml").error().message());
    println("{}", io::error(io::errc::closed, "read").message());
    println("{}", io::error(io::errc::invalid_path, "", "notes.txt").message());
    println("{}", io::error(io::errc::line_too_long, "").message());
}
```

Output:

```text
open config.toml: No such file or directory
read: stream closed
notes.txt: invalid path
line too long
```

## See also

- [code](code.md), [op](op.md), [path](path.md): the parts of the text
- [sgcl::io::error](../error.md)
