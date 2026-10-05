[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [public_key](README.md)

# sgcl::net::ssh::public_key::certificate

```cpp
optional<ssh::certificate> certificate() const noexcept;
```

A certificate's fields ([certificate](../certificate.md)): the key it certifies, what it is for, its serial and id, its
principals, its validity, its options and extensions, the authority that signed it — once its signature verifies under
that authority's key. Whether the program trusts the authority, and the certificate for a name now, is its own to
decide, or [known_hosts](../known_hosts/README.md)'s and [authorized_keys](../authorized_keys/README.md)'s.

## Parameters

None.

## Return value

The fields; `nullopt` for a key that is not a certificate, or one whose signature does not verify.

## Complexity

Linear in the blob, and one signature's check.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key cert = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/host-cert.pub"));
    net::ssh::certificate c = cert.certificate().value();
    println("{} {} {}", c.key_id, c.serial, c.type == net::ssh::certificate_type::host);
    for (auto& p : c.principals) {
        println("{}", p);
    }
    println("{}", c.signature_key.fingerprint());
}
```

Output:

```text
host-id 7 true
localhost
127.0.0.1
SHA256:RTKk3cp6z2M2m0RDLveLa0TZGxzMn0LzF5l7kAo0u+c
```

## See also

- [certificate](../certificate.md)
- [is_certificate](is_certificate.md)
- [sgcl::net::ssh::public_key](README.md)
