[sgcl](../../README.md) › [slog](../README.md) › [record](README.md)

# sgcl::slog::record::time

```cpp
time::datetime time() const noexcept;
```

Returns the time of the record, taken when it was made, in the logger's zone: the local one, or UTC when the logger
was made with [options](../options.md)`::utc`.

## Parameters

None.

## Return value

The time, a [datetime](../../time/datetime/README.md) to the nanosecond the clock gives.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger(slog::options{.handler = kept, .utc = true}).info("now");
    auto r = kept.records()[0];
    println("{}", r.time().zone() == time::zone::utc());
    println("{}", r.time().format("%F %T"));
}
```

Sample output:

```text
true
2026-09-28 12:05:01.123456000
```

## See also

- [options](../options.md): `utc`
- [sgcl::slog::record](README.md)
