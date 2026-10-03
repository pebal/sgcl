[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::nanosecond

```cpp
int nanosecond() const noexcept;
```

The part of the second at the instant, in nanoseconds, Go's `t.Nanosecond()`: 0 to 999999999. It rounds down, as
[unix](unix.md) does: half a second before 1970, 1969-12-31T23:59:59.5Z, is second 59 and nanosecond 500000000.

## Parameters

None.

## Return value

The nanoseconds past the second, 0 to 999999999.

## Complexity

Logarithmic in the number of the zone's changes; constant in UTC and in a fixed zone.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto t = time::datetime::from_unix_nano(1790246475122575000, time::zone::utc());
    println(t.nanosecond());
    auto half = time::datetime::from_unix_milli(-500, time::zone::utc());
    println("{} {}", half.second(), half.nanosecond());
}
```

Output:

```text
122575000
59 500000000
```

## See also

- [second](second.md): the whole second
- [unix_nano](unix_nano.md): the whole count
- [sgcl::time::datetime](README.md)
