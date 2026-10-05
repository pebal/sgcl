[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md)

# sgcl::net::ssh::public_key

```cpp
#include "sgcl/net/ssh/keys.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    class public_key;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::public_key` is a public key of SSH, or an OpenSSH certificate over one: its blob, the wire form of RFC 4253
§6.6 ([bytes](bytes.md)), and the comment its text had. It is read from the text of an authorized_keys or .pub line,
`ssh-ed25519 AAAA… user@host` ([parse](parse.md), or the constructor from text), or from a blob
([from_bytes](from_bytes.md)), and written back the same way ([to_string](to_string.md)); its
[fingerprint](fingerprint.md) is the one ssh-keygen prints. A certificate's fields — the key it certifies, its
principals, validity and authority — are read by [certificate](certificate.md), its signature checked. Go's
`ssh.PublicKey` and `ssh.ParseAuthorizedKey`.

A value: copied freely, compared by its blob (the comment aside).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](public_key.md) | an empty key, or a key from its text |
| [parse](parse.md) | a key from an authorized_keys or .pub line (static) |
| [from_bytes](from_bytes.md) | a key from its blob (static) |
| [type](type.md) | the kind of key |
| [type_name](type_name.md) | the name of its type |
| [bytes](bytes.md) | the blob |
| [comment](comment.md) | the comment of its text |
| [with_comment](with_comment.md) | the key with another comment |
| [fingerprint](fingerprint.md) | ssh-keygen's fingerprint |
| [to_string](to_string.md) | the key's line |
| [is_certificate](is_certificate.md) | whether it is a certificate |
| [certificate](certificate.md) | a certificate's fields |
| [verify](verify.md) | whether a signature signs data under the key |
| [operator bool](operator_bool.md) | whether there is a key |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | whether two keys are the same key |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519.pub"));
    println("{} {} {}", key.type_name(), key.fingerprint(), key.comment());
    net::ssh::public_key cert = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/host-cert.pub"));
    println("{} {}", cert.is_certificate(), cert.certificate()->principals[0]);
}
```

Output:

```text
ssh-ed25519 SHA256:PN89yHvZV5qRnQP3eclDnzJi7J8IfYfMitxf9jaYMn0 test-ed25519
true localhost
```

## See also

- [private_key](../private_key/README.md), [certificate](../certificate.md)
- [known_hosts](../known_hosts/README.md), [authorized_keys](../authorized_keys/README.md)
- [net::ssh](../README.md)
