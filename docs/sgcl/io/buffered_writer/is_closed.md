[sgcl](../../README.md) › [io](../README.md) › [buffered_writer](README.md)

# sgcl::io::buffered_writer::is_closed

```cpp
bool is_closed() const noexcept;
```

Checks whether the writer was closed: whether [close](close.md) or `async_close` was called, whatever its result. A
closed writer refuses every write with `errc::closed`.

## Parameters

None.

## Return value

`true` after a close, `false` before.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer out;
    io::buffered_writer w(out);
    println("{}", w.is_closed());
    w.close();
    println("{}", w.is_closed());
}
```

Output:

```text
false
true
```

## See also

- [close](close.md): closes the writer
- [sgcl::io::buffered_writer](README.md)
