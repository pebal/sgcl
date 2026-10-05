[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::file_info

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    using file_time = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

    struct file_info;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`io::file_info` is what [stat](../stat.md) says about a path, Go's `fs.FileInfo` as a plain struct: the name, the size,
the type, the permissions and the time of the last modification. [lstat](../lstat.md),
[file::stat](../file/stat.md) and [directory_entry::info](../directory_entry/info.md) return one too. `file_time`, the type
of `modified`, is the time of the system clock in nanoseconds, the resolution the file systems keep.

## Rules

- It is a value read once: a later change of the file is not seen in it.

## Member objects

| Member | Description |
|---|---|
| `string name` | the last element of the path, as [path::base](../path/base.md) gives it |
| `uint64_t size` | the size in bytes; for a directory or a link, what the system reports; 0 by default |
| `file_type type` | the [file_type](../file_type.md); `file_type::unknown` by default |
| `permissions mode` | the [permissions](../permissions.md), the set-id and sticky bits included; `permissions::none` by default |
| `file_time modified` | the time of the last modification |

## Member functions

| Function | Description |
|---|---|
| [is_regular](is_regular.md) | checks whether the file is a regular file |
| [is_directory](is_directory.md) | checks whether the file is a directory |
| [is_symlink](is_symlink.md) | checks whether the file is a symbolic link |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("photo.jpg", "not really a photo");
    (void)io::chmod("photo.jpg", io::permissions(0640));
    if (auto info = io::stat("photo.jpg")) {
        println("{}: {} bytes, mode {:o}", info->name, info->size, unsigned(info->mode));
        auto age = std::chrono::system_clock::now() - info->modified;
        println("{}", age < std::chrono::minutes(1));
    }
}
```

Output:

```text
photo.jpg: 18 bytes, mode 640
true
```

## See also

- [stat](../stat.md), [lstat](../lstat.md): what makes one
- [set_modified](../set_modified.md): sets `modified`
- [directory_entry](../directory_entry/README.md): an entry of a listing
