[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [problem](README.md)

# sgcl::net::acme::problem::code

```cpp
errc code() const noexcept;
```

The type of the problem as a code of the acme category ([errc](../errc.md)): one of RFC 8555 §6.7, RFC 9773's
`alreadyReplaced` or the profiles' `invalidProfile`, by its name after `urn:ietf:params:acme:error:`; anything else
`errc::unknown_problem`.

## Parameters

None.

## Return value

The code.

## Complexity

Linear in the length of the type.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::problem p;
    p.type = "urn:ietf:params:acme:error:rateLimited";
    println("{}", p.code() == net::acme::errc::rate_limited);
    p.type = "about:blank";
    println("{}", p.code() == net::acme::errc::unknown_problem);
}
```

Output:

```text
true
true
```

## See also

- [errc](../errc.md)
- [sgcl::net::acme::problem](README.md)
