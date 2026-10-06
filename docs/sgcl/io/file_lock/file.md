[sgcl](../../README.md) › [io](../README.md) › [file_lock](README.md)

# sgcl::io::file_lock::file

```cpp
const io::file& file() const noexcept;
```

The file the guard locked, a handle it keeps for as long as it lives, so that the descriptor, and the lock with it,
stays open; the file of the path form ([lock_file](../lock_file.md)`(path)`), to write to or to close. Empty for a
guard made by the default constructor.

## Parameters

None.

## Return value

The file.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file_lock held = io::lock_file("pid.lock").value();
    io::file f = held.file();  // the same file: a handle
    f.write(to_string(io::pid() > 0));
    f.seek(0);
    println("{}", f.read_all_text().value());
}
```

Output:

```text
true
```

## See also

- [file](../file/README.md)
- [sgcl::io::file_lock](README.md)
