[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ip_address](README.md)

# sgcl::crypto::x509::operator==, operator!= (sgcl::crypto::x509::ip_address)

```cpp
friend bool operator==(const ip_address& a, const ip_address& b) noexcept;
```

Compares two addresses: equal when they are of the same size and their used bytes are equal. An IPv4 address and its
IPv4-mapped form are of different sizes, and differ. `!=` is its negation, written by the compiler from `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the addresses to compare |

## Return value

`true` when the addresses are equal, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a leaf of the tree's test CA
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);

    crypto::x509::ip_address loopback{.size = 4};
    loopback.bytes[0] = byte(127);
    loopback.bytes[3] = byte(1);
    println("{}", cert.ip_addresses()[0] == loopback);
    println("{}", cert.ip_addresses()[1] != loopback);
}
```

Output:

```text
true
true
```

## See also

- [sgcl::crypto::x509::ip_address](README.md)
