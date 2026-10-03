[sgcl](../README.md) › [net](README.md)

# sgcl::net::lookup_category

```cpp
#include "sgcl/net/error.h"   // or "sgcl/net.h"

namespace sgcl::net {
    const std::error_category& lookup_category() noexcept;
}
```

Returns the error category of the codes `getaddrinfo` and `getnameinfo` return, the `EAI_*` values, named
`"lookup"`; its `message` is `gai_strerror`'s text. The `EAI_*` values are not `errno` values and would be misread
in the system category: on macOS `EAI_AGAIN` is 2, which is `ENOENT` there. So a code from the resolver is in this
category, never in the system one, but for two: `EAI_NONAME` (and `EAI_NODATA` where it exists) is
[errc](errc.md)`::host_not_found`, and `EAI_SYSTEM` is the `errno` it stands for. One object, made at the first
call.

## Parameters

None.

## Return value

The category, the same object at every call.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

#include <netdb.h>

using namespace sgcl;

int main() {
    error_code again(EAI_AGAIN, net::lookup_category());
    println("{}: {}", again.category().name(), again.message());
    println("{}", again == std::errc::no_such_file_or_directory);
}
```

Output:

```text
lookup: Temporary failure in name resolution
false
```

## See also

- [dns](dns.md): the lookups whose failures these are
- [category](category.md): the module's own codes
