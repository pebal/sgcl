[sgcl](../../README.md) › [io](../README.md) › [file](../file.md)

# sgcl::io::file::fd

```cpp
int fd() const noexcept;
```

The descriptor of the file, for a call of the system that the library does not make. The file keeps owning it: the
descriptor is not closed by the caller, and not used after the file is closed.

## Parameters

None.

## Return value

The descriptor, or `-1` once the file is closed ([close](close.md)).

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include <sys/file.h>

using namespace sgcl;

int main() {
    io::file f = io::create("app.lock");
    println("{}", ::flock(f.fd(), LOCK_EX | LOCK_NB) == 0);
    f.close();
    println("{}", f.fd());
}
```

Output:

```text
true
-1
```

## See also

- [from_fd](../from_fd.md): a file over a descriptor opened elsewhere
- [close](close.md): gives the descriptor back
- [sgcl::io::file](../file.md)
