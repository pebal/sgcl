[sgcl](../../README.md) › [time](../README.md) › [zone](../zone.md)

# sgcl::time::zone::name

```cpp
string name() const noexcept;
```

The name of the zone: the name of the database it was loaded by (`"Europe/Warsaw"`), `"UTC"`, a fixed offset's
(`"+05:30"`), the TZ string of a zone made from one, the name given to [from_tzif](from_tzif.md). The
[local](local.md) zone has the name of the file of the database it is; one read from `/etc/localtime` that is not
a link into the database is `"Local"`.

## Parameters

None.

## Return value

The name.

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
    println("{}", time::zone("America/New_York").name());
    println("{}", time::zone::utc().name());
    println("{}", time::zone::fixed(-(9 * hour + 30 * minute)).name());
    println("{}", time::zone::from_posix("EST5EDT,M3.2.0,M11.1.0").value().name());
}
```

Output:

```text
America/New_York
UTC
-09:30
EST5EDT,M3.2.0,M11.1.0
```

## See also

- [abbreviation_at](abbreviation_at.md): what the zone's clock shows
- [sgcl::time::zone](../zone.md)
