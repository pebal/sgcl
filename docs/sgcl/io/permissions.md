[sgcl](../README.md) › [io](README.md)

# sgcl::io::permissions

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    enum class permissions : unsigned {
        none = 0,
        owner_read = 0400, owner_write = 0200, owner_exec = 0100,
        group_read = 040, group_write = 020, group_exec = 010,
        others_read = 04, others_write = 02, others_exec = 01,
        set_uid = 04000, set_gid = 02000, sticky = 01000,
        all = 0777
    };

    constexpr permissions operator|(permissions a, permissions b) noexcept;
    constexpr permissions operator&(permissions a, permissions b) noexcept;
}
```

The mode bits of a file: the read, write and execute bits of the owner, the group and the others, and the set-id and
sticky bits, as POSIX numbers them, Go's `fs.FileMode` without the type bits. A mode is written as the octal number
of the shell, `permissions(0644)`, or as the names joined by `|`; `&` tests a bit. What [stat](stat.md) reports as
`mode`, and what [mkdir](mkdir.md), [chmod](chmod.md), [open](open.md) and [write_file](write_file.md) take.

| Value | Description |
|---|---|
| `none` | no bit, `0` |
| `owner_read` | the owner may read, `0400` |
| `owner_write` | the owner may write, `0200` |
| `owner_exec` | the owner may execute, or search a directory, `0100` |
| `group_read` | the group may read, `040` |
| `group_write` | the group may write, `020` |
| `group_exec` | the group may execute, `010` |
| `others_read` | the others may read, `04` |
| `others_write` | the others may write, `02` |
| `others_exec` | the others may execute, `01` |
| `set_uid` | the program runs as its owner, `04000` |
| `set_gid` | the program runs as its group; in a directory, new files take its group, `02000` |
| `sticky` | in a directory, only the owner of a file may remove it, `01000` |
| `all` | the read, write and execute bits of all three, `0777` |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    using enum io::permissions;
    io::permissions mode = owner_read | owner_write | group_read | others_read;
    println("{:o} {}", unsigned(mode), mode == io::permissions(0644));

    (void)io::write_file("shared.txt", "x");
    (void)io::chmod("shared.txt", io::permissions(0666));
    if (auto info = io::stat("shared.txt")) {
        println("world-writable: {}", (info->mode & others_write) != none);
    }
}
```

Output:

```text
644 true
world-writable: true
```

## See also

- [chmod](chmod.md): sets the bits of a file
- [file_info](file_info.md): `mode`, the bits read
- [mkdir](mkdir.md), [open](open.md): the bits of what they make
