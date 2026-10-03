[sgcl](../../README.md) › [io](../README.md) › [shared_memory](README.md)

# sgcl::io::shared_memory::create

```cpp
static expected<shared_memory, error> create(const string& name, size_t size) noexcept;
```

Makes a new object of `size` bytes under the name and maps it for reading and writing: `shm_open` with `O_CREAT`
and `O_EXCL`, `ftruncate` and `mmap` on POSIX, `CreateFileMappingW` on Windows. The bytes are zeros, and the object
is readable and writable by the user's own processes alone (`0600`). On POSIX the object and its name outlive the
process until [remove](remove.md): a program that may find a leftover of a crash removes the name first. A failure
after the object was made removes it again.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name: not empty, without `/` or `\`; at most 30 characters on macOS |
| `size` | the size of the object in bytes, not 0 |

## Return value

The handle of the mapped region, or an [error](../error/README.md) with the operation `create` and the name as its path:

- `is_exists()` when the name is taken;
- `std::errc::invalid_argument` for a size of 0;
- `errc::invalid_path` for a bad name (empty, or with `/` or `\`);
- `ENAMETOOLONG` for a name longer than the system takes;
- the error of `shm_open`, `ftruncate` or `mmap` otherwise (the operation `mmap` for the last).

## Complexity

Constant: the pages are given to the object when they are touched.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::shared_memory::remove("sgcl-example-create");
    io::shared_memory region = io::shared_memory::create("sgcl-example-create", 100);
    println("{} {}", region.size(), int(region.data()[99]));
    println("{}", io::shared_memory::create("sgcl-example-create", 100).error().message());
    println("{}", io::shared_memory::create("sgcl-example-empty", 0).error().message());
    println("{}", io::shared_memory::create("a/b", 100).error().message());
    (void)io::shared_memory::remove("sgcl-example-create");
}
```

Output:

```text
100 0
create sgcl-example-create: File exists
create sgcl-example-empty: Invalid argument
create a/b: invalid path
```

## See also

- [open](open.md): the object mapped by another process
- [remove](remove.md): the name taken away
- [sgcl::io::shared_memory](README.md)
