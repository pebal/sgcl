[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::as_date

```cpp
optional<time::date> as_date() const noexcept;
```

The [date](../../time/date/README.md) of a local date, a local date-time or an offset date-time (as its text
shows it, in its offset); `nullopt` for a local time and every other value.

## Parameters

None.

## Return value

The date, or `nullopt`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/time.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto v = encoding::toml::parse("a = 1979-05-27\nb = 1979-05-27T23:30:00-08:00\nc = 07:32:00").value();
    println("{} {} {}", v["a"].as_date(), v["b"].as_date(), v["c"].as_date());
}
```

Output:

```text
1979-05-27 1979-05-27 nullopt
```

## See also

- [as_datetime](as_datetime.md)
- [sgcl::encoding::toml](README.md)
