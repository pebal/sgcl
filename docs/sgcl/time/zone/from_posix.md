[sgcl](../../README.md) › [time](../README.md) › [zone](../zone.md)

# sgcl::time::zone::from_posix

```cpp
static expected<zone, error> from_posix(const string& rule) noexcept;
```

A zone from a POSIX TZ string alone, named by it: `"CET-1CEST,M3.5.0,M10.5.0/3"`, the rule of Central Europe.
The string is POSIX's, with the two extensions of RFC 9636: hours of a change from -167 to 167, and daylight
saving time all year (`"EST5EDT,0/0,J365/25"`). A name of daylight saving time with no rule after it takes
tzcode's default, the rule of the United States since 2007.

The zone lives while a zone or a datetime has it, and while it lives the same string gives the same zone. Once
nothing has it, it is gone, and its entry in the registry with it: a server that makes a zone of every string a
client sends keeps only the ones still in use.

## Parameters

| Parameter | Description |
|---|---|
| `rule` | the TZ string |

## Return value

The zone, or an [error](../error.md) with a sentence and the byte where the part that failed starts:

- `"a POSIX TZ string expected"`: an empty string;
- `"a name of three or more letters, or one in <>, expected"`: the name of the standard time;
- `"an offset expected: [+|-]hh[:mm[:ss]], hours 0 to 24"`: an offset;
- `"a name of daylight saving time expected"`;
- `"a comma and the rule expected"`;
- `"the date the daylight saving time starts expected: Jn, n or Mm.w.d, then /time"`;
- `"a comma and the date it ends expected"`;
- `"the date the daylight saving time ends expected: Jn, n or Mm.w.d, then /time"`;
- `"the end of the TZ string expected"`: something after the rule.

## Complexity

Linear in the length of `rule`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone cet = time::zone::from_posix("CET-1CEST,M3.5.0,M10.5.0/3").value();
    auto winter = time::date(2026, 1, 15).at(12, 0, cet);
    auto summer = time::date(2026, 7, 15).at(12, 0, cet);
    println("{} {} {}", cet.name(), cet.abbreviation_at(winter), cet.abbreviation_at(summer));
    println("{}", cet.next_transition(winter).value());

    time::zone us = time::zone::from_posix("EST5EDT").value();
    println("{}", us.next_transition(winter).value());

    auto bad = time::zone::from_posix("CET-1CEST,M3.5.0");
    println("{} (byte {})", bad.error().message(), bad.error().offset());
}
```

Output:

```text
CET-1CEST,M3.5.0,M10.5.0/3 CET CEST
2026-03-29T03:00:00+02:00
2026-03-08T03:00:00-04:00
a comma and the date it ends expected (byte 16)
```

## See also

- [from_tzif](from_tzif.md): a zone with its history, from a TZif file
- [fixed](fixed.md): an offset alone
- [sgcl::time::zone](../zone.md)
