[sgcl](../README.md) › [io](README.md)

# sgcl::io::cache_dir

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<string, error> cache_dir() noexcept;
}
```

Returns the directory the platform names for the cached data of programs, Go's `os.UserCacheDir`:
`~/Library/Caches` on macOS; elsewhere `$XDG_CACHE_HOME` when it is set and not empty, else `~/.cache`. A program
keeps its own under it, `io::path::join(*io::cache_dir(), "myapp")`. The directory is named, not made.

## Parameters

None.

## Return value

The path, or the error of [home_dir](home_dir.md) when the home is needed and has none.

## Complexity

Linear in the size of the environment and the length of the path.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    if (auto dir = io::cache_dir()) {
        println("{}", io::path::join(*dir, "myapp"));
    }
}
```

## See also

- [config_dir](config_dir.md): the directory of configuration
- [home_dir](home_dir.md): the home directory
- [mkdir_all](mkdir_all.md): makes the directory and its parents
