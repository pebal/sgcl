[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::set_date

```cpp
email& set_date(const time::datetime& t);
```

The Date field, written as RFC 5322 writes a date in the datetime's zone.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the instant |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;


int main() {
    encoding::email m;
    m.set_date(time::datetime::from_unix(0, time::zone::utc()));
    println("{}", m.header("Date"));
}
```

Output:

```text
Thu, 01 Jan 1970 00:00:00 +0000
```

## See also

- [date](date.md)
- [email](README.md)
