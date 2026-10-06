[sgcl](../README.md) › [io](README.md)

# sgcl::io::open_library

```cpp
#include "sgcl/io/library.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<library, error> open_library(const string& path, const library_options& options = {}) noexcept;
}
```

Loads a dynamic library (`dlopen`; `LoadLibraryExW` on Windows): a path, or a name the dynamic loader looks for in
its own paths (the system's, `DYLD_LIBRARY_PATH`, `LD_LIBRARY_PATH`); [library_file_name](library_file_name.md) makes
the platform's file name of one. Every function it uses is bound at once and its symbols stay its own, unless the
[library_options](library_options.md) say otherwise.

## Parameters

| Parameter | Description |
|---|---|
| `path` | a path, or a file name for the loader to find |
| `options` | lazy binding, symbols for the libraries loaded after it |

## Return value

The [library](library/README.md), or the [error](error/README.md), its operation `open_library`: `errc::library`
with the dynamic loader's text as the error's path (the system's error code on Windows), so that `message()` reads
`open_library <the loader's text>: dynamic library error`.

## Complexity

The loader's: the file mapped, its dependencies loaded, its functions bound.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto z = io::open_library(io::library_file_name("z"));
    println("{}", z.has_value());
    auto none = io::open_library("libnot-there.dylib");
    println("{}", none.error().code() == io::errc::library);
}
```

Output:

```text
true
true
```

## See also

- [library](library/README.md), [library_options](library_options.md)
