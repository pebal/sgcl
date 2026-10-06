[sgcl](../../README.md) › [io](../README.md) › [library](README.md)

# sgcl::io::library::close

```cpp
expected<void, error> close() const noexcept;
```

Unloads the library (`dlclose`): once; a second call does nothing and succeeds. A pointer taken from it — a function,
a variable — is not to be used after it. The collector never unloads a library: one never closed stays loaded to the
end of the process.

## Parameters

None.

## Return value

Nothing, or the [error](../error/README.md), its operation `close`: `errc::library` with the loader's text.

## Complexity

The loader's.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::library z = io::open_library(io::library_file_name("z")).value();
    println("{}", z.close().has_value());
    println("{}", z.symbol<const char*()>("zlibVersion").error().code() == io::errc::closed);
}
```

Output:

```text
true
true
```

## See also

- [is_closed](is_closed.md)
- [sgcl::io::library](README.md)
