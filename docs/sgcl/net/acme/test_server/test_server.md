[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [test_server](README.md)

# sgcl::net::acme::test_server::test_server

```cpp
test_server();                                // (1)
explicit test_server(const options& o);       // (2)
test_server(test_server&& other) noexcept;    // (3)
```

1. A server with the default [options](../test_server-options.md), listening at once on a port of the system's choice
   of the loopback.
2. The same with the options `o`.
3. The server of `other`, which is left moved-from.

Its CA is made with it: a P-256 root (and one more per alternate chain) and an intermediate that issues.

## Parameters

| Parameter | Description |
|---|---|
| `o` | what the server is and how it validates |
| `other` | the server to take |

## Complexity

The CA's keys and certificates made, a listener.

## Exceptions

`std::runtime_error` when it cannot listen at `o.address`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca;
    net::acme::test_server other = std::move(ca);
    println("{}", other.directory_url().starts_with("http://127.0.0.1:"));
}
```

Output:

```text
true
```

## See also

- [options](../test_server-options.md)
- [sgcl::net::acme::test_server](README.md)
