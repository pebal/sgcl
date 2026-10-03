[sgcl](../README.md) › [io](README.md)

# sgcl::io::open_flags

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io.h"

namespace sgcl::io {
    enum class open_flags : unsigned {
        read = 1,
        write = 2,
        create = 4,
        truncate = 8,
        append = 16,
        exclusive = 32,
        sync = 64
    };

    constexpr open_flags operator|(open_flags a, open_flags b) noexcept;
    constexpr bool operator&(open_flags a, open_flags b) noexcept;
}
```

How [open](open.md) opens a file: a set of flags, combined with `|`; `a & b` tells whether the two share a flag.
`read` is the default. `write` alone truncates nothing and creates nothing (an error when the file is not there), so
a file to write is opened with `write | create | truncate`, which [create](create.md) spells, or
`write | create | append` for a log. Every file is opened `O_CLOEXEC` besides: its descriptor is closed in a
program the process executes.

| Value | Description |
|---|---|
| `read` | for reading (`O_RDONLY`, or `O_RDWR` with `write`) |
| `write` | for writing (`O_WRONLY`, or `O_RDWR` with `read`) |
| `create` | the file is created when it is not there, with the permissions `open` is given (`O_CREAT`) |
| `truncate` | the file is emptied when it is opened (`O_TRUNC`) |
| `append` | every write goes to the end of the file (`O_APPEND`) |
| `exclusive` | with `create`, an error when the file exists, `is_exists()` (`O_EXCL`) |
| `sync` | every write reaches the disk before it returns (`O_SYNC`) |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto log = io::open_flags::write | io::open_flags::create | io::open_flags::append;
    println("{} {}", log & io::open_flags::append, log & io::open_flags::truncate);

    io::open("app.log", log).value().write("started\n");
    io::open("app.log", log).value().write("stopped\n");
    print("{}", *io::read_text("app.log"));

    auto missing = io::open("other.log", io::open_flags::write);
    println("{}", missing.error().is_not_found());
}
```

Output:

```text
true false
started
stopped
true
```

## See also

- [open](open.md): what takes them
- [create](create.md): `write | create | truncate`
- [sgcl::io::file](file.md)
