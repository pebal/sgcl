[sgcl](../../README.md) › [slog](../README.md) › [rotating_file](README.md)

# sgcl::slog::rotating_file::size

```cpp
uint64_t size() const noexcept;
```

Returns the bytes in the current file: what was there at the open or at the last reopen, and what was written
since the last rotation. A write of another thread at that moment counts once it ends.

## Parameters

None.

## Return value

The bytes.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::rotating_file out = slog::rotating_file::open("app.log");
    out.write("hello\n");
    println("{}", out.size());
}
```

Output:

```text
6
```

## See also

- [path](path.md)
- [sgcl::slog::rotating_file](README.md)
