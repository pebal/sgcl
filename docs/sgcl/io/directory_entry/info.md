[sgcl](../../README.md) › [io](../README.md) › [directory_entry](README.md)

# sgcl::io::directory_entry::info

```cpp
expected<file_info, error> info() const noexcept;
```

Returns what the file system says about the entry now: the [lstat](../lstat.md) of its `path`, which describes a
symbolic link itself. Go's `DirEntry.Info`. The listing gave the name and the type alone; the size, the permissions
and the time of the last modification cost this call.

## Parameters

None.

## Return value

The [file_info](../file_info/README.md), or the [error](../error/README.md) of the `lstat`: the entry may have been removed since
the listing.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir("logs");
    (void)io::write_file("logs/a.log", "12345");
    (void)io::write_file("logs/b.log", "123");
    auto entries = io::read_dir("logs");
    if (!entries) {
        return 1;
    }
    uint64_t total = 0;
    for (const io::directory_entry& e : *entries) {
        if (auto i = e.info()) {
            total += i->size;
        }
    }
    println("{} bytes", total);
}
```

Output:

```text
8 bytes
```

## See also

- [is_directory](is_directory.md): the type without a stat
- [stat](../stat.md), [lstat](../lstat.md): the same of any path
- [sgcl::io::directory_entry](README.md)
