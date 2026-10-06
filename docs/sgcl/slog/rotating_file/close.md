[sgcl](../../README.md) › [slog](../README.md) › [rotating_file](README.md)

# sgcl::slog::rotating_file::close

```cpp
expected<void, io::error> close() const;
```

Closes the current file and stops the SIGHUP watch: a write, a rotation or a reopen after it is `errc::closed`. A
second close does nothing. A file never closed is closed by the collector once nothing holds it.

## Parameters

None.

## Return value

Nothing, or the `io::error` of the close.

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
    out.close();
    println("{}", out.write("late\n").error().is_closed());
}
```

Output:

```text
true
```

## See also

- [write](write.md)
- [sgcl::slog::rotating_file](README.md)
