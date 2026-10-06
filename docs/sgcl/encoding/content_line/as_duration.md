[sgcl](../../README.md) › [encoding](../README.md) › [content_line](README.md)

# sgcl::encoding::content_line::as_duration, as_utc_offset

```cpp
optional<duration> as_duration() const noexcept;      // (1)
optional<duration> as_utc_offset() const noexcept;    // (2)
```

1. A DURATION of RFC 5545 §3.3.6: weeks (`P2W`), or days and a time (`P1DT2H30M`, `-PT15M`), the parts in their order
   and none left out between the first and the last.
2. A UTC-OFFSET: `+0530`, `-080000` (`-0000` is not one).

## Parameters

None.

## Return value

The value, or `nullopt`.

## Complexity

Linear in the size of the value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} {} {}", encoding::content_line("TRIGGER", "-PT15M").as_duration(), encoding::content_line("D", "P1DT2H").as_duration(),
            encoding::content_line("D", "P1H").as_duration());
    println(encoding::content_line("TZOFFSETTO", "+0530").as_utc_offset());
}
```

Output:

```text
-15m0s 26h0m0s nullopt
5h30m0s
```

## See also

- [as_date](as_date.md)
- [sgcl::encoding::content_line](README.md)
