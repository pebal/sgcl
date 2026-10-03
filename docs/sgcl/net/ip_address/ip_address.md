[sgcl](../../README.md) › [net](../README.md) › [ip_address](README.md)

# sgcl::net::ip_address::ip_address

```cpp
ip_address() noexcept = default;            // (1)
explicit ip_address(const string& text);    // (2)
```

Constructs an address.

1. The empty address, Go's zero `netip.Addr`: `is_valid()` is false, `is_v4()` and `is_v6()` too, and its text is
   `invalid IP`.
2. The address a literal in the program spells: what [parse](parse.md) reads, or `bad_expected_access<io::error>` with
   `parse`'s error. A text from outside the program (a setting, a request) is parsed, and its error is a value.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text of the address, as [parse](parse.md) reads it |

## Complexity

- (1) Constant.
- (2) Linear in the length of `text`.

## Exceptions

- (1) None.
- (2) `bad_expected_access<io::error>` when `text` is not an address; its `error()` is `parse`'s, of the code
  `net::errc::invalid_address`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::ip_address none;
    net::ip_address home("127.0.0.1");
    net::ip_address scoped("fe80::1%en0");
    println("{} {} {}", none, home, scoped);

    try {
        net::ip_address wrong("127.1");
    } catch (const bad_expected_access<io::error>& e) {
        println(e.error().message());
    }
}
```

Output:

```text
invalid IP 127.0.0.1 fe80::1%en0
parse IP address 127.1: invalid address
```

## See also

- [parse](parse.md): reads a text from outside the program
- [v4](v4.md), [v6](v6.md): an address of its bytes
- [sgcl::net::ip_address](README.md)
