[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::field::as_string

```cpp
optional<string> as_string() const noexcept;
```

The value of a string or of bytes.

## Parameters

None.

## Return value

The string; none for a value of another type.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    println("{}", *net::amqp::field("quorum").as_string());
}
```

Output:

```text
quorum
```

## See also

- [bytes](bytes.md)
- [field](README.md)
