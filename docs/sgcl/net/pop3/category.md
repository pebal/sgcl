[sgcl](../../README.md) › [net](../README.md) › [pop3](README.md)

# sgcl::net::pop3::category

```cpp
const std::error_category& category() noexcept;
```

The error category of [errc](errc.md), named `"pop3"`: one object for the program.

## Parameters

None.

## Return value

The category.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net/pop3.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println("{}", net::pop3::category().name());
}
```

Output:

```text
pop3
```

## See also

- [errc](errc.md)
- [pop3](README.md)
