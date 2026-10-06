[sgcl](../../README.md) › [net](../README.md) › [pop3](README.md)

# sgcl::net::pop3::make_error_code

```cpp
error_code make_error_code(errc e) noexcept;
```

An [errc](errc.md) as an `error_code` of the category `"pop3"`; what makes `error_code e = net::pop3::errc::in_use;` compile.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the code |

## Return value

The `error_code`.

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
    error_code e = net::pop3::make_error_code(net::pop3::errc::no_such_message);
    println("{}", e.message());
}
```

Output:

```text
no such message
```

## See also

- [errc](errc.md)
- [pop3](README.md)
