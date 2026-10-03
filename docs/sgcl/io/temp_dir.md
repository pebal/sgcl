[sgcl](../README.md) › [io](README.md)

# sgcl::io::temp_dir

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    string temp_dir() noexcept;
}
```

Returns the directory of temporary files, Go's `os.TempDir`: the variable `TMPDIR`, [cleaned](path/clean.md), when
it is set and not empty, else `/tmp`. It is where [temp_file](temp_file.md) and [make_temp_dir](make_temp_dir.md)
make what they make when they are given no directory.

## Parameters

None.

## Return value

The path.

## Complexity

Linear in the size of the environment and the length of the path.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::setenv("TMPDIR", "/var/tmp/app/");
    println("{}", io::temp_dir());
    (void)io::unsetenv("TMPDIR");
    println("{}", io::temp_dir());
}
```

Output:

```text
/var/tmp/app
/tmp
```

## See also

- [temp_file](temp_file.md), [make_temp_dir](make_temp_dir.md): a file and a directory of their own in it
- [cache_dir](cache_dir.md): the directory of cached data
