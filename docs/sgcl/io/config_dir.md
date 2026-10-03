[sgcl](../README.md) › [io](README.md)

# sgcl::io::config_dir

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<string, error> config_dir() noexcept;
}
```

Returns the directory the platform names for the configuration of programs, Go's `os.UserConfigDir`:
`~/Library/Application Support` on macOS; elsewhere `$XDG_CONFIG_HOME` when it is set and not empty, else
`~/.config`. A program keeps its own under it. The directory is named, not made.

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
    string level = io::env("LOG_LEVEL", "info");
    if (auto dir = io::config_dir()) {
        println("{} in {}", level, io::path::join(*dir, "myapp", "settings.json"));
    }
}
```

## See also

- [cache_dir](cache_dir.md): the directory of cached data
- [home_dir](home_dir.md): the home directory
- [env](env.md): a variable as a value with a fallback
