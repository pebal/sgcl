[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [private_key](README.md)

# sgcl::net::ssh::private_key::to_openssh

```cpp
crypto::secret_bytes to_openssh(const string& passphrase = {}) const noexcept;
```

The key in OpenSSH's file format, as ssh-keygen writes it: `-----BEGIN OPENSSH PRIVATE KEY-----`, the base64 in lines of
70 characters, the comment kept; unencrypted, or with a passphrase encrypted under bcrypt_pbkdf (16 rounds, a 16-byte
salt) and aes256-ctr, ssh-keygen's default. A `crypto::secret_bytes`: never managed memory.

## Parameters

| Parameter | Description |
|---|---|
| `passphrase` | the passphrase to encrypt with; empty: unencrypted |

## Return value

The file's text.

## Complexity

Linear in the key; with a passphrase, bcrypt_pbkdf's 16 rounds.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::private_key key = net::ssh::private_key::generate();
    crypto::secret_bytes text = key.to_openssh("passphrase");
    net::ssh::private_key back = net::ssh::private_key::parse(text, "passphrase");
    println("{}", back.public_key() == key.public_key());
}
```

Output:

```text
true
```

## See also

- [save](save.md), [parse](parse.md)
- [sgcl::net::ssh::private_key](README.md)
