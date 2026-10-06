[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [authentication_results](README.md)

# sgcl::net::smtp::authentication_results::operator==

```cpp
friend bool operator==(const authentication_results&, const authentication_results&) noexcept = default;
```

Whether two values say the same: the same authserv-id and the same results in the same order. Two fields that differ
in comments, folding or the case of methods parse to equal values.

## Parameters

None.

## Return value

`true` when every member is equal.

## Complexity

Linear in the results.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    net::smtp::authentication_results a("mx (comment); SPF=pass smtp.mailfrom=example.com");
    net::smtp::authentication_results b("mx; spf=pass\r\n smtp.mailfrom=example.com");
    println("{}", a == b);
}
```

Output:

```text
true
```

## See also

- [authentication_results](README.md)
