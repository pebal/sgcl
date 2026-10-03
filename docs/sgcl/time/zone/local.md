[sgcl](../../README.md) › [time](../README.md) › [zone](../zone.md)

# sgcl::time::zone::local

```cpp
static zone local() noexcept;
```

The zone of this computer:

- the `TZ` variable of the environment, if it is set: a name of the database (`":Europe/Warsaw"`,
  `"Europe/Warsaw"`), a path to a TZif file (named for its place in the database when it lies in one, `"Local"`
  otherwise), or a POSIX TZ string; empty means UTC, and so does one that names nothing;
- else the zone `/etc/localtime` is, named for the file of the database it links to, or `"Local"` when it is not
  a link into the database;
- else UTC.

It is settled once, the first time it is asked for, as Go settles `time.Local`, and kept for the rest of the
program: a later change of `TZ` changes nothing. Its [name](name.md) is the database's, `"Europe/Warsaw"`, where
Go's `time.Local` is named `"Local"`.

## Parameters

None.

## Return value

The local zone.

## Complexity

Constant after the first call; the first reads the environment and a file.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone here = time::zone::local();
    auto t = time::date(2026, 7, 1).at(12, 0, here);
    println("{} {} {}", here.name(), here.abbreviation_at(t), here.offset_at(t));
}
```

## See also

- [load](load.md): a zone by its name
- [sgcl::time::zone](../zone.md)
