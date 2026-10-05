[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [test_server](README.md)

# sgcl::net::acme::test_server::orders

```cpp
size_t orders() const noexcept;
```

How many orders the server made: what tells a test how many times a client went to the CA.

## Parameters

None.

## Return value

The count.

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
    net::acme::test_server ca({.skip_validation = true});
    net::acme::manager certificates({"example.com"},
                                    {.directory_url = ca.directory_url(), .accept_terms = true});
    certificates.certificate("example.com");
    certificates.certificate("example.com");   // from memory
    println("{}", ca.orders());
    certificates.close();
}
```

Output:

```text
1
```

## See also

- [certificates](certificates.md)
- [sgcl::net::acme::test_server](README.md)
