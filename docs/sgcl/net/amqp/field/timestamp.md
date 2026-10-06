[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::field::timestamp

```cpp
static field timestamp(const time::datetime& t) noexcept;
```

A timestamp ('T'): the time in whole seconds.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the time |


## Return value

The field.

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

- [as_timestamp](as_timestamp.md)
- [field](README.md)
