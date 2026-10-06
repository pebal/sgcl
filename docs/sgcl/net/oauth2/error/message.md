[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [error](README.md)

# sgcl::net::oauth2::error::message

```cpp
string message() const noexcept;
```

The error as one line: `code: description`, the code alone when there is no description, the
[io::error](../../../io/error/README.md)'s message for a failure of the exchange.

## Parameters

None.

## Return value

The line.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/oauth2.h"

using namespace sgcl;

int main() {
    println("{}", net::oauth2::error("access_denied", "the user said no").message());
    println("{}", net::oauth2::error("slow_down", "").message());
}
```

Output:

```text
access_denied: the user said no
slow_down
```

## See also

- [error](README.md)
