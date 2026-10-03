[sgcl](../README.md) › [io](README.md)

# sgcl::io::home_dir

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<string, error> home_dir() noexcept;
}
```

Returns the home directory of the user: the variable `HOME` when it is set and not empty, else the directory the
password database gives the process's user (`getpwuid`). Go's `os.UserHomeDir`, which reads `HOME` alone.

## Parameters

None.

## Return value

The path, or an [error](error.md) of `std::errc::no_such_file_or_directory` when neither names one; the operation
is `home_dir`.

## Complexity

Linear in the size of the environment, and a lookup in the password database when `HOME` is not set.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    if (auto home = io::home_dir()) {
        println("{}", *home);
    }
}
```

## See also

- [cache_dir](cache_dir.md), [config_dir](config_dir.md): the directories of a program under the home
- [temp_dir](temp_dir.md): the directory of temporary files
