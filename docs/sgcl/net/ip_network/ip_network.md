[sgcl](../../README.md) › [net](../README.md) › [ip_network](README.md)

# sgcl::net::ip_network::ip_network

```cpp
ip_network() noexcept = default;             // (1)
ip_network(ip_address address, int bits);    // (2)
explicit ip_network(const string& text);     // (3)
```

Constructs a network.

1. The empty network, Go's zero `netip.Prefix`: `is_valid()` is false, `bits()` is -1 and its text is `invalid
   Prefix`.
2. The network of `address` and the length `bits`, Go's `netip.PrefixFrom`. The address is kept as given, its zone
   dropped; the empty address gives the empty network, whatever `bits` is. A length out of range for the address is a
   broken contract and throws, where Go's `PrefixFrom` returns an invalid prefix.
3. The network a literal in the program spells: what [parse](parse.md) reads, or `bad_expected_access<io::error>` with
   `parse`'s error. A text from outside the program is parsed, and its error is a value.

## Parameters

| Parameter | Description |
|---|---|
| `address` | the address of the network, kept as given but for its zone |
| `bits` | the length of the prefix: 0 to 32 for IPv4, 0 to 128 for IPv6 |
| `text` | the text of the network, as [parse](parse.md) reads it |

## Complexity

- (1–2) Constant.
- (3) Linear in the length of `text`.

## Exceptions

- (1) None.
- (2) `invalid_argument` when `address` is valid and `bits` is out of its range.
- (3) `bad_expected_access<io::error>` when `text` is not a network; its `error()` is `parse`'s, of the code
  `net::errc::invalid_address`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::ip_network none;
    net::ip_network docs(net::ip_address("2001:db8::1%en0"), 32);
    net::ip_network lan("192.168.0.0/16");
    println("{} {} {}", none, docs, lan);

    try {
        net::ip_network wide(net::ip_address("10.0.0.0"), 33);
    } catch (const invalid_argument& e) {
        println(e.what());
    }
}
```

Output:

```text
invalid Prefix 2001:db8::1/32 192.168.0.0/16
sgcl::net::ip_network: the prefix length is out of range for the address
```

## See also

- [parse](parse.md): reads a text from outside the program
- [sgcl::net::ip_network](README.md)
