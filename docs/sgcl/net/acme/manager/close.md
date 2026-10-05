[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [manager](README.md)

# sgcl::net::acme::manager::close

```cpp
void close() const;
```

The renewals stopped, and nothing obtained any more: what is held is still served, a name without a certificate is
`io::errc::closed`. A manager that lives as long as its server needs no close; one of a test, or of a server that
stops, closes, so that no renewal waits in the background.

## Parameters

None.

## Return value

None.

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
    net::acme::manager certificates({"example.com", "www.example.com"},
                                    {.directory_url = ca.directory_url(), .accept_terms = true});

    certificates.certificate("example.com");
    certificates.close();
    println("{}", certificates.certificate("example.com").has_value());
    println("{}", certificates.certificate("www.example.com").error().code() == io::errc::closed);
}
```

Output:

```text
true
true
```

## See also

- [sgcl::net::acme::manager](README.md)
