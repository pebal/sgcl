[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [reply](README.md)

# sgcl::net::smtp::reply::positive

```cpp
bool positive() const noexcept;
```

Whether the code is a success (2xx) or a request to go on (3xx).

## Parameters

None.

## Return value

`true` for 200 to 399.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::smtp::reply ok{250}, go_on{354}, refused{550};
    println("{} {} {}", ok.positive(), go_on.positive(), refused.positive());
}
```

Output:

```text
true true false
```

## See also

- [reply](README.md)
