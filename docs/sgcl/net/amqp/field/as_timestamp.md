[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::field::as_timestamp

```cpp
optional<time::datetime> as_timestamp() const noexcept;
```

The value of a timestamp, in whole seconds, in UTC.

## Parameters

None.

## Return value

The time; none for a value of another type.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto t = net::amqp::field::timestamp(time::datetime::from_unix(1700000000, time::zone::utc()));
    println("{}", t.as_timestamp()->unix());
}
```

Output:

```text
1700000000
```

## See also

- [timestamp](timestamp.md)
- [field](README.md)
