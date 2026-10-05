[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::category

```cpp
const std::error_category& category() noexcept;
```

The error category of ACME, named `"acme"`: the category of every [errc](errc.md), whose message for a code is the
problem type's meaning, `"acme: rate limited"`.

## Parameters

None.

## Return value

The category, one object for the process.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    println("{}", net::acme::category().name());
    println("{}", net::acme::category().message(int(net::acme::errc::bad_csr)));
}
```

Output:

```text
acme
acme: the CSR is unacceptable
```

## See also

- [errc](errc.md), [make_error_code](make_error_code.md)
- [net::acme](README.md)
