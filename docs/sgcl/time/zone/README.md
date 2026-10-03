[sgcl](../../README.md) › [time](../README.md)

# sgcl::time::zone

```cpp
#include "sgcl/time/zone.h"   // or "sgcl/time.h"

namespace sgcl::time {
    class zone;
}
```

`sgcl::time::zone` is a time zone: the rules that give a place's clock its offset from UTC at every moment, the
abbreviation it shows (`CEST`) and whether it is daylight saving time. It is Go's `*time.Location`, Java's
`ZoneId`, the `const time_zone*` of C++20's `tzdb` — which the C++ library of this system does not have: libc++
ships `<chrono>`'s calendar but no time zones, so the zones here are the module's own, read from the files every
Unix system keeps.

There are five ways to one: [utc](utc.md) (and a default-constructed zone); [fixed](fixed.md), always so
far east of UTC; [load](load.md), a zone of the system's tz database by its name, `"Europe/Warsaw"`, with the
[constructor](zone.md) for a name the program itself writes, `time::zone warsaw("Europe/Warsaw")`;
[from_tzif](from_tzif.md), a zone from the bytes of a TZif file of any origin; and
[from_posix](from_posix.md), a zone from a POSIX TZ string alone. And [local](local.md), the zone of
this computer.

A zone is one word, a `tracked_ptr` to its data, so it lives where a `tracked_ptr` may — on a stack, in a managed
object, in a container of the library — as a [string](../../core/string/README.md) does, and a function of this module takes
one as `const zone&`. The zones of the database and the fixed offsets are kept for the rest of the program in a
registry, finitely many of both, as `std::chrono::tzdb` keeps its `time_zone`s: the second `load` of a name finds
the first one's zone and reads nothing, from any thread. A zone made from bytes or a string of any origin
(`from_tzif`, `from_posix`) is kept by the zones and datetimes that have it, and by nothing else: the collector
takes it with the last of them.

What a zone says about an instant is asked with a [datetime](../datetime/README.md): the offset, the abbreviation, daylight
saving time, and the changes of the zone strictly after and before the instant, which Go has no way to ask for. A
datetime asks its own zone the same (`t.offset()`, `t.abbreviation()`).

## Rules

- A zone is one word; `zone()`, UTC, is the null pointer and allocates nothing.
- Nothing waits once a zone is loaded. `load`, the constructor from a name, `local` and `available` read files on
  the thread that asks, `load` once for each name; a task that loads a zone of the database for the first time
  blocks its worker for the read of a file of some kilobytes.
- The reading is written from RFC 9636 (which replaced RFC 8536) and POSIX. A file is checked whole before
  anything is built from it, and a file that is not one is an [error](../error/README.md) with the byte it stopped on.
- A zone's table drops the transitions that change nothing and carries the rule of the file's footer to the year
  2100, so that every time to then is found by a binary search; a later one asks the rule.
- Before a zone's first transition its time is the file's first type (RFC 9636), the local mean time of the 19th
  century for most zones (Warsaw: `LMT`, +01:24); after the last, the footer's rule, or the last type where there
  is no footer.
- A transition is a change of the offset, of daylight saving time or of the abbreviation.
- Two zones are equal when they are the same zone: the same name loaded twice, the same offset, the same bytes
  under the same name, the same TZ string. `Poland` and `Europe/Warsaw`, one a link to the other, are two zones
  with the same rules.
- Windows has no directory of the database; there `load` finds nothing, and `from_tzif` takes a database one
  carries.

### From code written for Go

| With Go | With sgcl::time |
|---|---|
| `*time.Location` | `time::zone`, one word |
| `time.LoadLocation(name)` | `zone::load(name)`, loaded once per name where Go reads the file at every `LoadLocation`; a name the program writes is constructed, `time::zone z(name)`, which throws |
| `time.FixedZone(name, offset)` | `zone::fixed(offset)`, named for the offset |
| `time.UTC` | `zone::utc()` |
| `time.Local` | `zone::local()`, whose `name()` is the database's, `"Europe/Warsaw"`, not `"Local"` |
| `time.LoadLocationFromTZData(name, data)` | `zone::from_tzif(data, name)`; and `zone::from_posix(rule)`, which Go has not |
| — | `z.offset_at(t)`, `next_transition`, `previous_transition`, `zone::available()`: Go cannot list the changes of a zone or the zones |
| `time/tzdata`, a database built in | none: later, with Windows; `from_tzif` takes a database one carries |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](zone.md) | UTC, or the zone of a name the program writes |

#### Making a zone

| Function | Description |
|---|---|
| [utc](utc.md) | UTC (static) |
| [fixed](fixed.md) | a fixed offset from UTC (static) |
| [load](load.md) | a zone of the system's database by its name (static) |
| [from_tzif](from_tzif.md) | a zone from the bytes of a TZif file (static) |
| [from_posix](from_posix.md) | a zone from a POSIX TZ string (static) |
| [local](local.md) | the zone of this computer (static) |
| [available](available.md) | the names of the system's database (static) |

#### Lookup

| Function | Description |
|---|---|
| [offset_at](offset_at.md) | the offset from UTC at an instant |
| [abbreviation_at](abbreviation_at.md) | the abbreviation at an instant |
| [is_dst_at](is_dst_at.md) | whether it is daylight saving time at an instant |
| [next_transition](next_transition.md) | the first change after an instant |
| [previous_transition](previous_transition.md) | the last change before an instant |

#### Observers

| Function | Description |
|---|---|
| [name](name.md) | the name of the zone |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | whether two zones are the same zone |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    auto summer = time::date(2026, 7, 1).at(12, 0, warsaw);
    println("{} {} {} {}", warsaw.name(), warsaw.abbreviation_at(summer), warsaw.offset_at(summer),
            warsaw.is_dst_at(summer));

    // The changes around a time
    auto next = warsaw.next_transition(summer).value();
    auto previous = warsaw.previous_transition(summer).value();
    println("{} .. {}", previous, next);

    // Half an hour of daylight saving time, an offset of 45 minutes
    time::zone lord_howe("Australia/Lord_Howe");
    time::zone kathmandu("Asia/Kathmandu");
    println("{} {}", summer.in(lord_howe), summer.in(kathmandu));

    // A zone of a POSIX TZ string, and a fixed offset
    time::zone rule = time::zone::from_posix("CET-1CEST,M3.5.0,M10.5.0/3").value();
    println("{} {}", rule.offset_at(summer) == warsaw.offset_at(summer),
            time::zone::fixed(-(3 * hour + 30 * minute)).name());
}
```

Output:

```text
Europe/Warsaw CEST 2h0m0s true
2026-03-29T03:00:00+02:00 .. 2026-10-25T02:00:00+01:00
2026-07-01T20:30:00+10:30 2026-07-01T15:45:00+05:45
true -03:30
```

## See also

- [datetime](../datetime/README.md): an instant and the zone it is seen in
- [date::at](../date/at.md): a time of a day in a zone
- [error](../error/README.md): why a name or a file is not a zone
- [sgcl::time](../README.md)
