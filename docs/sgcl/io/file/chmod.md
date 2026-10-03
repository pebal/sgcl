[sgcl](../../README.md) › [io](../README.md) › [file](../file.md)

# sgcl::io::file::chmod, async_chmod

```cpp
expected<void, error> chmod(permissions p) const noexcept;                       // (1)
async::task<expected<void, error>> async_chmod(permissions p) const noexcept;    // (2)
```

Sets the permissions of the open file to `p`: the `fchmod(2)` of the descriptor. The umask does not apply.

1. On the calling thread.
2. The same for a task, on the [blocking pool](../../async/spawn_blocking.md).

## Parameters

| Parameter | Description |
|---|---|
| `p` | the new permissions ([permissions](../permissions.md)) |

## Return value

Nothing, or the [error](../error.md), its operation `chmod` and its path the file's: `errc::closed` for a closed
file, otherwise the `errno` of `fchmod(2)` (`EPERM` for a file of another user).

## Complexity

One system call.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file f = io::create("secret.txt");
    f.chmod(io::permissions::owner_read | io::permissions::owner_write);
    println("{}", f.stat()->mode == io::permissions(0600));
    f.chmod(io::permissions(0640));
    println("{}", f.stat()->mode == io::permissions(0640));
}
```

Output:

```text
true
true
```

## See also

- [permissions](../permissions.md): the bits
- [chmod](../chmod.md): the same of a path
- [sgcl::io::file](../file.md)
