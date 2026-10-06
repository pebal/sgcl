[sgcl](../../README.md) › [slog](../README.md) › [rotating_file](README.md)

# sgcl::slog::rotating_file::rotating_file

```cpp
rotating_file(const rotating_file&) noexcept = default;    // (1)
rotating_file(rotating_file&&) noexcept = default;         // (2)
```

1. A handle of the same file.
2. The same, taken from the other handle, which stands for the same file still.

A file is opened by [open](open.md), whose error is a value, as io's opens are: rotating_file has no constructor from
a path.

## Parameters

None.

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
    slog::rotating_file copy = out;
    copy.write("through the copy\n");
    println("{} {}", out == copy, out.size());
}
```

Output:

```text
true 17
```

## See also

- [open](open.md)
- [operator==](operator_cmp.md)
- [sgcl::slog::rotating_file](README.md)
