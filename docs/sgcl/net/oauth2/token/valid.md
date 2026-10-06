[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [token](README.md)

# sgcl::net::oauth2::token::valid

```cpp
bool valid() const noexcept;
```

Whether the token can be used: its access token is not empty, and it has no `expiry` or one more than 10 seconds away (a
token that dies on its way is no use), as Go's `Token.Valid`.

## Parameters

None.

## Return value

`true` when the token can be sent.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/oauth2.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    using namespace std::chrono_literals;
    net::oauth2::token t;
    println("{}", t.valid());  // no access token
    t.access_token = "at-1";
    println("{}", t.valid());  // no expiry
    t.expiry = time::now() + 5s;
    println("{}", t.valid());  // within the margin
    t.expiry = time::now() + 1h;
    println("{}", t.valid());
}
```

Output:

```text
false
true
false
true
```

## See also

- [token_source::token](../token_source/token.md)
- [token](README.md)
