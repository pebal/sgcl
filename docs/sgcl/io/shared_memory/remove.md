[sgcl](../../README.md) › [io](../README.md) › [shared_memory](../shared_memory.md)

# sgcl::io::shared_memory::remove

```cpp
static expected<void, error> remove(const string& name) noexcept;
```

Takes the name away (`shm_unlink`). The processes that mapped the object keep it, and its bytes, until their regions
go; a [create](create.md) of the name then makes a new object, and an [open](open.md) finds none until it does. On
POSIX an object lives until it is removed, past the end of every process that used it, until a reboot: a program
that creates one removes it when done. On Windows the object lives until the last handle to it, a mapped view
included, is closed, and `remove` has nothing to do: it succeeds.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the object |

## Return value

Nothing, or an [error](../error.md) with the operation `remove` and the name as its path: `is_not_found()` when there
is no object of the name, `errc::invalid_path` for a bad name, the error of `shm_unlink` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::shared_memory::remove("sgcl-example-remove");
    io::shared_memory region = io::shared_memory::create("sgcl-example-remove", 64);
    region.data()[0] = byte(7);
    println("{}", bool(io::shared_memory::remove("sgcl-example-remove")));
    println("{}", int(region.data()[0]));
    println("{}", io::shared_memory::remove("sgcl-example-remove").error().message());
}
```

Output:

```text
true
7
remove sgcl-example-remove: No such file or directory
```

## See also

- [create](create.md): makes an object under the name
- [close](close.md): the region given back, the name kept
- [sgcl::io::shared_memory](../shared_memory.md)
