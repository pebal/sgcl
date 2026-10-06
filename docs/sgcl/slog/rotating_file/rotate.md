[sgcl](../../README.md) › [slog](../README.md) › [rotating_file](README.md)

# sgcl::slog::rotating_file::rotate

```cpp
expected<void, io::error> rotate() const;
```

Rotates the file now, whatever its size and the time: renamed to `name-YYYY-MM-DDTHH-MM-SS.mmm.ext`, the path
opened anew, the gzip and the removal of the oldest queued on the blocking pool. A rotation in progress on another
thread makes this one nothing.

## Parameters

None.

## Return value

Nothing, or the `io::error` of the rename or of the open; `errc::closed` after [close](close.md).

## Complexity

A rename and an open.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::rotating_file out = slog::rotating_file::open("app.log", {.keep = 0});
    out.write("before\n");
    out.rotate();
    out.write("after\n");
    println("{}", out.size());
}
```

Output:

```text
6
```

## See also

- [write](write.md)
- [rotation](../rotation.md)
- [sgcl::slog::rotating_file](README.md)
