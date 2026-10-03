[sgcl](../../README.md) › [io](../README.md) › [error](README.md)

# sgcl::io::error::is_exists

```cpp
bool is_exists() const noexcept;
```

Checks whether the operation failed because something is at the path already: `EEXIST`
(`std::errc::file_exists`), whatever the category it is reported in. A [mkdir](../mkdir.md) of a directory that is
there, a file opened with `open_flags::exclusive` that exists, a [shared_memory](../shared_memory/README.md) created
under a name that is taken. Go's `errors.Is(err, fs.ErrExist)`.

## Parameters

None.

## Return value

`true` when the code is `EEXIST`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto made = io::mkdir(".");
    println("{}: exists? {}", made.error().message(), made.error().is_exists());
}
```

Output:

```text
mkdir .: File exists: exists? true
```

## See also

- [is_not_found](is_not_found.md)
- [sgcl::io::error](README.md)
