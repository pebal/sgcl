[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::make_error_code

```cpp
#include "sgcl/net/imap/error.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    error_code make_error_code(errc e) noexcept;
}
```

Returns `e` as an `error_code` of the category `"imap"` ([category](category.md)): what the specialization of
`std::is_error_code_enum` makes an `errc` convert to, so that `e.code() == net::imap::errc::no` compares and an
`io::error` is built from a code.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the code |

## Return value

The `error_code` of the value of `e` in the module's category.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    error_code code = net::imap::make_error_code(net::imap::errc::already_exists);
    println("{} {}", code.category().name(), code.message());
    io::error e(net::imap::errc::too_big, "APPEND", "INBOX");
    println("{}", e.message());
}
```

Output:

```text
imap the mailbox already exists
APPEND INBOX: too big
```

## See also

- [errc](errc.md), [category](category.md)
- [sgcl::net::imap](README.md)
