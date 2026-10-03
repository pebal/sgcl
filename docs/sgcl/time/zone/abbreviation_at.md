[sgcl](../../README.md) › [time](../README.md) › [zone](README.md)

# sgcl::time::zone::abbreviation_at

```cpp
string abbreviation_at(const datetime& t) const noexcept;
```

The abbreviation the zone's clock shows at the instant `t`: `"CEST"` in Warsaw in summer, `"CET"` in winter, the
offset as the file writes it where the database has no letters for a zone (`"+11"`). UTC's is `"UTC"`, a
[fixed](fixed.md) zone's its name. A datetime asks its own zone the same, `t.abbreviation()`.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the instant |

## Return value

The abbreviation.

## Complexity

Logarithmic in the number of the zone's transitions up to the year 2100, as [offset_at](offset_at.md).

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    time::zone utc = time::zone::utc();
    auto summer = time::date(2026, 7, 1).at(12, 0, utc);
    auto winter = time::date(2026, 1, 1).at(12, 0, utc);
    println("{} {}", warsaw.abbreviation_at(summer), warsaw.abbreviation_at(winter));
    println("{}", warsaw.abbreviation_at(time::date(1850, 1, 1).at(0, 0, utc)));
    println("{}", time::zone("Australia/Lord_Howe").abbreviation_at(winter));
    time::zone fixed = time::zone::fixed(-3 * hour);
    println("{} {}", utc.abbreviation_at(summer), fixed.abbreviation_at(summer));
}
```

Output:

```text
CEST CET
LMT
+11
UTC -03:00
```

## See also

- [offset_at](offset_at.md): the offset at an instant
- [name](name.md): the name of the zone
- [sgcl::time::zone](README.md)
