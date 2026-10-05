[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [authorized_keys](README.md)

# sgcl::net::ssh::authorized_keys::find

```cpp
optional<entry> find(const ssh::public_key& key) const noexcept;
```

The line of a key: a plain key's own line (one without `cert-authority`), or, for a certificate whose signature
verifies, the `cert-authority` line of the authority that signed it. Whether the certificate holds for a user is
[allows](allows.md)'s to decide; the options are the program's to apply.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key a user offers |

## Return value

The line ([entry](../authorized_keys-entry/README.md)); `nullopt` when there is none.

## Complexity

Linear in the lines.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key ca = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ca.pub"));
    net::ssh::public_key cert = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519-cert.pub"));
    net::ssh::authorized_keys keys = net::ssh::authorized_keys::parse("cert-authority,principals=\"admin\" " + ca.to_string());
    println("{}", keys.find(cert)->option("principals"));
}
```

Output:

```text
admin
```

## See also

- [allows](allows.md)
- [sgcl::net::ssh::authorized_keys](README.md)
