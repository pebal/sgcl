[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [test_server](README.md)

# sgcl::net::acme::test_server::set_renewal_window

```cpp
void set_renewal_window(const time::datetime& start, const time::datetime& end) const;
```

The window renewalInfo suggests for every certificate from now on, in place of its default (from two thirds of a
certificate's lifetime, for a ninth of it): a window in the past is what a CA answers to have certificates replaced at
once.

## Parameters

| Parameter | Description |
|---|---|
| `start`, `end` | the window |

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
    net::acme::manager certificates({"example.com"},
                                    {.directory_url = ca.directory_url(), .accept_terms = true});
    auto now = time::now();
    ca.set_renewal_window(now - std::chrono::hours(2), now - std::chrono::hours(1));
    certificates.certificate("example.com");
    while (ca.certificates() < 2) {   // renewed in the background, at once
        async::sleep(std::chrono::milliseconds(10)).wait();
    }
    println("renewed");
    certificates.close();
}
```

Output:

```text
renewed
```

## See also

- [client::renewal_info](../client/renewal_info.md)
- [sgcl::net::acme::test_server](README.md)
