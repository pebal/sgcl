[sgcl](../../README.md) › [slog](../README.md) › [rotating_file](README.md)

# sgcl::slog::rotating_file::path

```cpp
string path() const noexcept;
```

Returns the path the file was opened at: where the current file is, whatever it has been rotated to.

## Parameters

None.

## Return value

The path.

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
    println("{}", out.path());
}
```

Output:

```text
app.log
```

## See also

- [size](size.md)
- [sgcl::slog::rotating_file](README.md)
