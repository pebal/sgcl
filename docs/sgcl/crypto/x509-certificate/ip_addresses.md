[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::ip_addresses

```cpp
const vector<ip_address>& ip_addresses() const noexcept;
```

Returns the IP addresses of the subject alternative name, in the order of the certificate, as bytes: 4 for IPv4, 16
for IPv6. An IPv4-mapped address (`::ffff:a.b.c.d`) is given as its 4 bytes, as name constraints and
[verify_ip](verify_ip.md) take it (Go keeps the 16 bytes).

## Parameters

None.

## Return value

The [ip_address](../x509-ip_address/README.md) values, empty when there are none.

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

    for (auto& a : cert.ip_addresses()) {
        println("{} bytes, the last {}", a.size, int(a.bytes[a.size - 1]));
    }
}
```

Output:

```text
4 bytes, the last 1
16 bytes, the last 1
```

## See also

- [verify_ip](verify_ip.md): checks an address against them
- [sgcl::crypto::x509::certificate](README.md)
