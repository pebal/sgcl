[sgcl](../../README.md) › [io](../README.md) › [buffered_writer](../buffered_writer.md)

# sgcl::io::buffered_writer::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a writer. It holds none when default-constructed, and every operation on it is then a
contract violation; a writer made over a stream is held from its constructor on, closed or not.

## Parameters

None.

## Return value

`true` when the handle holds a writer, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_writer out;
    println("{}", bool(out));
    out = io::buffered_writer(io::stdout);
    out.close();
    println("{}", bool(out));
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](buffered_writer.md): makes a writer, or an empty handle
- [is_closed](is_closed.md): checks whether the writer was closed
- [sgcl::io::buffered_writer](../buffered_writer.md)
