[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [test_server](README.md)

# sgcl::net::acme::test_server::certificates

```cpp
size_t certificates() const noexcept;
```

How many certificates the server issued.

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
    net::acme::manager certificates({"a.example", "b.example"},
                                    {.directory_url = ca.directory_url(), .accept_terms = true});
    certificates.certificate("a.example");
    certificates.certificate("b.example");
    println("{}", ca.certificates());
    certificates.close();
}
```

Output:

```text
2
```

## See also

- [orders](orders.md)
- [sgcl::net::acme::test_server](README.md)
