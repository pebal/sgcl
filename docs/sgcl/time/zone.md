# sgcl::time::zone

```cpp
#include "sgcl/time/zone.h"   // or "sgcl/time/time.h"

namespace sgcl::time {
    class zone;   // UTC, a fixed offset, or a zone of the tz database; a tracked pointer to its data
}
```

A time zone: the rules that give a place's clock its offset from UTC at every moment, the abbreviation it shows (`CEST`) and whether it is daylight saving time. Go's `*time.Location`, Java's `ZoneId`, the `const time_zone*` of C++20's `tzdb` — which the C++ library of this system does not have: libc++ ships `<chrono>`'s calendar but no time zones, so the zones here are the module's own, read from the files every Unix system keeps.

There are five ways to one:

- `zone::utc()` (and a default-constructed `zone`);
- `zone::fixed(offset)`: always so far east of UTC, named for it (`"+05:30"`);
- `zone::load(name)`: a zone of the system's tz database by its name, `"Europe/Warsaw"`, `"America/New_York"`, read from `/usr/share/zoneinfo` (on macOS a link to `/var/db/timezone/zoneinfo`; the other places systems keep it are tried after) the first time and from memory every time after;
- `zone::from_tzif(bytes, name)`: a zone from the bytes of a TZif file of any origin (versions 1 to 4, RFC 9636), a database of one's own, one carried over the network;
- `zone::from_posix(rule)`: a zone from a POSIX TZ string alone, `"CET-1CEST,M3.5.0,M10.5.0/3"`.

And `zone::local()`, the zone of this computer: the `TZ` variable of the environment if it is set (`":Europe/Warsaw"`, `"Europe/Warsaw"`, a path to a TZif file, a POSIX TZ string; empty means UTC; one that names nothing, UTC), else the zone `/etc/localtime` is, named for the file of the database it links to, else UTC. It is settled once, the first time it is asked for, as Go settles `time.Local`.

A zone is one word: a `tracked_ptr` to its data, so it lives where a `tracked_ptr` may — on a stack, in a managed object, in a container of the library — as a [string](../core/string.md) does, and a function of this module takes one as `const zone&`. `zone()`, UTC, is the null pointer and allocates nothing. The zones of the database and the fixed offsets are kept for the rest of the program in a registry, finitely many of both, as `std::chrono::tzdb` keeps its `time_zone`s: the second `load` of a name finds the first one's zone and reads nothing, from any thread. A zone made from bytes or a string of any origin (`from_tzif`, `from_posix`) is kept by the zones and datetimes that have it, and by nothing else: the collector takes it with the last of them. Two zones are equal when they are the same zone: the same name loaded twice, the same offset, the same bytes under the same name; `Poland` and `Europe/Warsaw`, one a link to the other, are two zones with the same rules.

The reading is written from RFC 9636 (which replaced RFC 8536) and POSIX. A file is checked whole before anything is built from it — the counts against the length, the transitions in order, every index in range, every designation ended — and a file that is not one is an error with the byte it stopped on; the footer of versions 2 and later is the POSIX TZ string that gives the time after the last transition, with version 3's hours from -167 to 167 and daylight saving time all year (`"EST5EDT,0/0,J365/25"`). A zone's table drops the transitions that change nothing and carries the footer's rule to the year 2100, so that every time to then is found by a binary search; a later one asks the rule.

What a zone says about an instant is asked of a [datetime](datetime.md): `z.offset_at(t)`, `z.abbreviation_at(t)`, `z.is_dst_at(t)`, and `z.next_transition(t)`, `z.previous_transition(t)` — the changes of the zone strictly after and before `t`, which Go has no way to ask for. A datetime asks its own zone the same (`t.offset()`, `t.abbreviation()`).

## Rules

- Nothing waits once a zone is loaded. `load`, `local()` and `available()` read files on the thread that asks, once for each name; a task that loads a zone of the database for the first time blocks its worker for the read of a file of some kilobytes.
- A name of the database is letters, digits, `/`, `_`, `-`, `+` and `.`, without `..` and without a leading `/` or `.`: `load("../../etc/passwd")` is an error, not a read. `"UTC"` is `zone::utc()`.
- Before a zone's first transition its time is the file's first type (RFC 9636), the local mean time of the 19th century for most zones (Warsaw: `LMT`, +01:24); after the last, the footer's rule, or the last type where there is no footer.
- A zone made by `from_tzif` or `from_posix` lives while a zone or a datetime has it, and while it lives the same bytes under the same name, or the same string, give the same zone. Once nothing has it, it is gone, and its entry in the registry with it (swept by the threads that make zones, a pass spread over the insertions that made the dead entries): a server that makes a zone of every string a client sends keeps only the ones still in use.
- `fixed(offset)` takes whole seconds (the rest dropped) of less than a day either way, and throws `invalid_argument` otherwise, as a mistake of the program; `fixed(0)` is UTC. Its name is the offset, `"+05:30"`, `"-03:30"`, `"+05:30:15"` where there are seconds.
- A transition is a change of the offset, of daylight saving time or of the abbreviation. `next_transition` and `previous_transition` are nothing for UTC and a fixed offset, before the first change of a zone and after the last change of one that stopped changing (`Asia/Tokyo` since 1951).
- `available()` lists the files of the database that start with `TZif`, sorted, leaving out the copies of the whole database for other uses (`posix/`, `right/`) and `posixrules`: 598 names on this machine.
- A file of the `right/` kind (with leap seconds counted in its times) is read as POSIX time, its leap-second records checked and passed over, as Go and `std::chrono::sys_time` treat time.
- Windows has no such directory; there `load` finds nothing, and `from_tzif` takes a database one carries.

## Members

```cpp
constexpr zone() noexcept;                                  // UTC
static constexpr zone utc() noexcept;
static zone fixed(duration offset);                         // whole seconds, |offset| < 24h
static expected<zone, error> load(const string& name);      // "Europe/Warsaw", from the system's database
static expected<zone, error> from_tzif(const slice<const byte>& data, const string& name);
static expected<zone, error> from_posix(const string& rule);
static zone local();                                        // TZ, /etc/localtime, UTC; settled once
static vector<string> available();                          // the names of the database, sorted

string name() const;                                        // "Europe/Warsaw", "UTC", "+05:30", the TZ string
duration offset_at(const datetime& t) const;                // +2h in Warsaw in summer
string abbreviation_at(const datetime& t) const;            // "CEST"; UTC "UTC", a fixed offset its name
bool is_dst_at(const datetime& t) const;
optional<datetime> next_transition(const datetime& t) const;       // strictly after t, in this zone
optional<datetime> previous_transition(const datetime& t) const;   // strictly before t
// ==
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include "sgcl/time/time.h"
#include <iostream>

using namespace sgcl;

int main() {
    auto warsaw = time::zone::load("Europe/Warsaw").value();
    auto summer = time::date(2026, 7, 1).at(12, 0, warsaw);
    std::cout << warsaw.name() << " " << warsaw.abbreviation_at(summer) << " " << warsaw.offset_at(summer)
              << " " << warsaw.is_dst_at(summer) << "\n";

    // The changes around a time, which Go cannot list
    auto next = warsaw.next_transition(summer).value();
    auto previous = warsaw.previous_transition(summer).value();
    std::cout << previous << " .. " << next << "\n";

    // Half an hour of daylight saving time, an offset of 45 minutes
    auto lord_howe = time::zone::load("Australia/Lord_Howe").value();
    auto kathmandu = time::zone::load("Asia/Kathmandu").value();
    std::cout << summer.in(lord_howe) << " " << summer.in(kathmandu) << "\n";

    // A zone of a POSIX TZ string, and a fixed offset
    auto rule = time::zone::from_posix("CET-1CEST,M3.5.0,M10.5.0/3").value();
    std::cout << (rule.offset_at(summer) == warsaw.offset_at(summer)) << " "
              << time::zone::fixed(-(3 * hour + 30 * minute)).name() << "\n";

    for (auto name : {"Europe/Warsw", "../../etc/passwd"}) {
        auto z = time::zone::load(name);
        std::cout << z.error().message() << "\n";
    }
    std::cout << (time::zone::available().size() > 400) << "\n";
    return 0;
}
```

Output:

```text
Europe/Warsaw CEST 2h0m0s 1
2026-03-29T03:00:00+02:00 .. 2026-10-25T02:00:00+01:00
2026-07-01T20:30:00+10:30 2026-07-01T15:45:00+05:45
1 -03:30
unknown time zone "Europe/Warsw"
not a name of a time zone: "../../etc/passwd"
1
```

## See also

- [datetime](datetime.md): an instant and the zone it is seen in
- [time](README.md): the module, its errors, the table of Go's names
