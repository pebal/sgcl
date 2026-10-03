[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::unix_milli

```cpp
int64_t unix_milli() const noexcept;
```

The milliseconds since 1970-01-01T00:00:00Z: Go's `t.UnixMilli()`. The division rounds down, toward the past, as
[unix](unix.md)'s does: a quarter of a millisecond before 1970 is `-1`.

## Parameters

None.

## Return value

The whole milliseconds since 1970, negative before it, rounded down.

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
    auto t = time::datetime::from_unix_nano(1790246475122575000, time::zone::utc());
    println(t.unix_milli());
    println(time::datetime::from_unix_micro(-250, time::zone::utc()).unix_milli());
}
```

Output:

```text
1790246475122
-1
```

## See also

- [from_unix_milli](from_unix_milli.md): the datetime of the milliseconds
- [unix](unix.md), [unix_micro](unix_micro.md), [unix_nano](unix_nano.md): the other units
- [sgcl::time::datetime](README.md)
