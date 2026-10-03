[sgcl](../../README.md) › [time](../README.md) › [zone](../zone.md)

# sgcl::time::zone::load

```cpp
static expected<zone, error> load(const string& name) noexcept;
```

A zone of the system's tz database by its name: `"Europe/Warsaw"`, `"America/New_York"`. It is read from
`/usr/share/zoneinfo` (on macOS a link to `/var/db/timezone/zoneinfo`; the other places systems keep the database,
`/usr/share/lib/zoneinfo`, `/usr/lib/locale/TZ` and `/etc/zoneinfo`, are tried after) the first time, and from
memory every time after: the zone is kept in the registry for the rest of the program, and the second `load` of a
name finds the first one's zone and reads nothing, from any thread. `"UTC"` is [utc](utc.md), read from nowhere.

A name of the database is letters, digits, `/`, `_`, `-`, `+` and `.`, without `..`, without an empty part and
without a leading `/` or `.`: `load("../../etc/passwd")` is an error, not a read. The copy of the database under
`right/`, which counts leap seconds in its times, is not POSIX time, which a datetime is, and its names are
refused. Windows has no such directory; there `load` finds nothing.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the zone in the database, from a setting, the user, a file |

## Return value

The zone, or an [error](../error.md):

- `not a name of a time zone: "…"`: a name that is not one of the form above;
- `unknown time zone "…"`: no file of that name in any of the places;
- `time zone "…": ` and the system's message: a file that could not be read (a directory);
- `time zone "…": TZif: ` and why: a file that is not a TZif file, or not a whole one, with the byte of the file
  where the reading stopped ([from_tzif](from_tzif.md)).

## Complexity

Linear in the size of the zone's file the first time a name is loaded; a lookup of the name after that.

## Exceptions

None.

## Notes

A name the program itself writes is constructed, `time::zone warsaw("Europe/Warsaw")`
([constructor](zone.md)), and a wrong one throws; a name from outside is loaded, and its error is a value. A task
that loads a zone of the database for the first time blocks its worker for the read of a file of some kilobytes.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    for (const char* name : {"America/New_York", "Europe/Warsw", "../../etc/passwd", "zone.tab"}) {
        auto z = time::zone::load(name);
        if (z) {
            println("{}", z->name());
        } else {
            println("{} (byte {})", z.error().message(), z.error().offset());
        }
    }
}
```

Output:

```text
America/New_York
unknown time zone "Europe/Warsw" (byte 0)
not a name of a time zone: "../../etc/passwd" (byte 0)
time zone "zone.tab": TZif: not a TZif file (no "TZif" at the start) (byte 0)
```

## See also

- [(constructor)](zone.md): a name the program writes
- [available](available.md): the names there are
- [from_tzif](from_tzif.md): a zone from a database of one's own
- [sgcl::time::zone](../zone.md)
