[sgcl](../../README.md) › [net](../README.md) › [endpoint](README.md)

# sgcl::net::endpoint::endpoint

```cpp
endpoint() noexcept = default;                           // (1)
endpoint(ip_address address, uint16_t port) noexcept;    // (2)
explicit endpoint(const string& text);                   // (3)
```

Constructs an endpoint.

1. The empty endpoint, Go's zero `netip.AddrPort`: the empty address and the port 0; `is_valid()` is false and its
   text is `invalid AddrPort`.
2. The endpoint of `address` and `port`, Go's `netip.AddrPortFrom`. The address is kept as it is, its zone among it.
3. The endpoint a literal in the program spells: what [parse](parse.md) reads, or `bad_expected_access<io::error>`
   with `parse`'s error. A text from outside the program is parsed, and its error is a value.

## Parameters

| Parameter | Description |
|---|---|
| `address` | the address |
| `port` | the port |
| `text` | the text of the endpoint, as [parse](parse.md) reads it |

## Complexity

- (1–2) Constant.
- (3) Linear in the length of `text`.

## Exceptions

- (1–2) None.
- (3) `bad_expected_access<io::error>` when `text` is not an endpoint; its `error()` is `parse`'s, of the code
  `net::errc::invalid_address`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::endpoint none;
    net::endpoint any(net::ip_address::any_v6(), 8080);
    net::endpoint scoped("[fe80::1%en0]:22");
    println("{} {} {}", none, any, scoped);

    try {
        net::endpoint named("localhost:80");
    } catch (const bad_expected_access<io::error>& e) {
        println(e.error().message());
    }
}
```

Output:

```text
invalid AddrPort [::]:8080 [fe80::1%en0]:22
parse endpoint localhost:80: invalid address
```

## See also

- [parse](parse.md): reads a text from outside the program
- [sgcl::net::endpoint](README.md)
