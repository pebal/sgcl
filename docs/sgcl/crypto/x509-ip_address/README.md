[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md)

# sgcl::crypto::x509::ip_address

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    struct ip_address {
        array<byte, 16> bytes{};
        uint8_t size = 0;
    };
}
```

`sgcl::crypto::x509::ip_address` is an IP address of a certificate, an iPAddress of its subject alternative names
(RFC 5280 §4.2.1.6): the bytes in network order, `size` of them used, 4 or 16. As bytes and never as text, as Go
keeps it: the crypto module parses no address text and does not depend on `net`, whose `ip_address` converts from and
to it through its bytes.

## Rules

- A plain value of seventeen bytes: it lives anywhere and holds nothing.

## Member objects

| Member | Description |
|---|---|
| `bytes` | the address in network order, the first `size` bytes used, the rest zero |
| `size` | 4 for IPv4, 16 for IPv6; 0 by default |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | compare the addresses |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a leaf of the tree's test CA
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);

    for (auto& a : cert.ip_addresses()) {
        vector<int> parts;
        for (int i : range(int(a.size))) {
            parts.push_back(int(a.bytes[i]));
        }
        println("{}", parts);
    }
}
```

Output:

```text
[127, 0, 0, 1]
[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1]
```

## See also

- [certificate::ip_addresses](../x509-certificate/ip_addresses.md)
- [ip_range](../x509-ip_range/README.md): a range of name constraints
- [sgcl::crypto::x509](../x509.md)
