[sgcl](../../README.md) › [time](../README.md) › [zone](../zone.md)

# sgcl::time::operator== (sgcl::time::zone)

```cpp
friend bool operator==(const zone& a, const zone& b) noexcept;
```

Whether two zones are the same zone: the same name loaded twice, the same offset, the same bytes under the same
name, the same TZ string. `!=` is made from it by the compiler. A default-constructed zone, `utc()`,
`load("UTC")` and `fixed` of zero are all UTC. Zones with the same rules under two names are two zones: `Poland`
and `Europe/Warsaw`, one a link to the other, and a zone made by [from_tzif](from_tzif.md) and one loaded by the
same name from the same file.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the zones compared |

## Return value

`true` when the two are the same zone.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    println("{}", warsaw == time::zone::load("Europe/Warsaw").value());
    println("{}", warsaw == time::zone("Poland"));
    println("{}", time::zone() == time::zone::fixed(0 * hour));
    println("{}", time::zone::fixed(90 * minute) == time::zone::fixed(5400 * second));
}
```

Output:

```text
true
false
true
true
```

## See also

- [name](name.md): the name of a zone
- [sgcl::time::zone](../zone.md)
