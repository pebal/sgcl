[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::unix_micro

```cpp
int64_t unix_micro() const noexcept;
```

The microseconds since 1970-01-01T00:00:00Z: Go's `t.UnixMicro()`. The division rounds down, toward the past, as
[unix](unix.md)'s does.

## Parameters

None.

## Return value

The whole microseconds since 1970, negative before it, rounded down.

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
    auto t = time::datetime::from_unix_nano(1790246475122575999, time::zone::utc());
    println(t.unix_micro());
    println(time::datetime::from_unix_nano(-1, time::zone::utc()).unix_micro());
}
```

Output:

```text
1790246475122575
-1
```

## See also

- [from_unix_micro](from_unix_micro.md): the datetime of the microseconds
- [unix](unix.md), [unix_milli](unix_milli.md), [unix_nano](unix_nano.md): the other units
- [sgcl::time::datetime](../datetime.md)
