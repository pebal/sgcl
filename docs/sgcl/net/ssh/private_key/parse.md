[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [private_key](README.md)

# sgcl::net::ssh::private_key::parse

```cpp
static expected<private_key, io::error> parse(const slice<const byte>& text, const string& passphrase = {}) noexcept;
```

A key from a key file's text: OpenSSH's format when its BEGIN line is there (encrypted with the passphrase when it is:
bcrypt_pbkdf with aes256-ctr, aes192-ctr, aes128-ctr, aes256-gcm or aes128-gcm), PEM's otherwise (`PRIVATE KEY` of
PKCS #8, `EC PRIVATE KEY` of SEC 1, `RSA PRIVATE KEY` of PKCS #1, unencrypted). The text is read where it lies: a
`crypto::secret_bytes` of [crypto::read_secret](../../../crypto/read_secret.md), a buffer of the program's (a string
converts too, but its bytes are managed: the program's choice). An OpenSSH key's comment is kept.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the key file's text |
| `passphrase` | the passphrase of an encrypted key; empty: none |

## Return value

The key. Or the [io::error](../../../io/error/README.md), op `ssh key`, of crypto's codes: `malformed` for a text that is
no key of these forms (or whose check words, padding or public half do not agree), `unsupported` for a cipher, a KDF
or a key type not read here, `authentication` for an encrypted key with no passphrase or a wrong one.

## Complexity

Linear in the text; an encrypted key's bcrypt_pbkdf takes its rounds (16 by ssh-keygen: about a tenth of a second).

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    crypto::secret_bytes text = crypto::read_secret("tests/net/ssh/testdata/p256");
    net::ssh::private_key key = net::ssh::private_key::parse(text);
    println("{} {}", key.public_key().type_name(), key.comment());
    auto broken = net::ssh::private_key::parse("-----BEGIN OPENSSH PRIVATE KEY-----\n");
    println("{}", broken.error().code() == crypto::errc::malformed);
}
```

Output:

```text
ecdsa-sha2-nistp256 test-p256
true
```

## See also

- [load](load.md)
- [to_openssh](to_openssh.md)
- [sgcl::net::ssh::private_key](README.md)
