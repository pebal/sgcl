[sgcl](../../README.md) › [time](../README.md) › [zone](README.md)

# sgcl::time::zone::from_tzif

```cpp
static expected<zone, error> from_tzif(const slice<const byte>& data, const string& name) noexcept;
```

A zone from the bytes of a TZif file of any origin, versions 1 to 4 (RFC 9636, which replaced RFC 8536): a
database of one's own, one carried over the network, the one a program takes along to Windows, which has none.

The file is checked whole before anything is built from it — the counts against the length, the transitions in
order, every index in range, every designation ended — and a file that is not one is an error with the byte it
stopped on. The footer of versions 2 and later is the POSIX TZ string that gives the time after the last
transition ([from_posix](from_posix.md)), with version 3's hours from -167 to 167 and daylight saving time all
year. A file of the `right/` kind, with leap seconds counted in its times, is read as POSIX time, its leap-second
records checked and passed over, as Go and `std::chrono::sys_time` treat time.

The zone lives while a zone or a datetime has it, and while it lives the same bytes under the same name give the
same zone. Once nothing has it, it is gone, and its entry in the registry with it (swept by the threads that make
zones, a pass spread over the insertions that made the dead entries): a server that makes a zone of every file a
client sends keeps only the ones still in use.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `name` | the name of the zone, what [name](name.md) returns |

## Return value

The zone, or an [error](../error/README.md) whose sentence starts with `TZif:` and whose offset is the byte of the file
where the reading stopped: `"TZif: not a TZif file (no \"TZif\" at the start)"`, `"TZif: the header is cut
short"`, `"TZif: the transition times are not in ascending order"`, and for a footer that is not a TZ string,
`"TZif: the footer is not a POSIX TZ string: "` and [from_posix](from_posix.md)'s sentence, at the footer's byte.

## Complexity

Linear in the size of `data`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto bytes = io::read_file("/usr/share/zoneinfo/Europe/Warsaw").value();
    time::zone warsaw = time::zone::from_tzif(bytes, "Warsaw").value();
    auto summer = time::date(2026, 7, 1).at(12, 0, warsaw);
    println("{} {} {}", warsaw.name(), summer, warsaw.abbreviation_at(summer));
    println("{}", time::zone::from_tzif(bytes, "Warsaw").value() == warsaw);

    auto cut = time::zone::from_tzif(slice<const byte>(bytes.data(), 30), "Warsaw");
    println("{} (byte {})", cut.error().message(), cut.error().offset());
}
```

Output:

```text
Warsaw 2026-07-01T12:00:00+02:00 CEST
true
TZif: the header is cut short (byte 30)
```

## See also

- [load](load.md): a zone of the system's database
- [from_posix](from_posix.md): a zone from a TZ string alone
- [sgcl::time::zone](README.md)
