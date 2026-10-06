[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md)

# sgcl::net::ssh::private_key

```cpp
#include "sgcl/net/ssh/keys.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    class private_key;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::private_key` is a private key of SSH: Ed25519, ECDSA on P-256, P-384 or P-521, or RSA ([key_type](../key_type.md)).
It is read from a key file's text ([parse](parse.md), [load](load.md)) in OpenSSH's own format — `-----BEGIN OPENSSH
PRIVATE KEY-----`, what ssh-keygen writes, unencrypted or encrypted with a passphrase (bcrypt_pbkdf with aes256-ctr,
ssh-keygen's default, or AES-GCM) — or in PEM's (PKCS #8, SEC 1, PKCS #1, unencrypted), made from random bytes
([generate](generate.md)), and written back in OpenSSH's format ([to_openssh](to_openssh.md), [save](save.md)). Its
[public_key](public_key.md) is what an authorized_keys file or a server's host key line holds; it signs for a client's
authentication and a server's host key.

A key is a handle of one word whose key lives in unmanaged memory: copies of the handle share it, nothing copies its
bytes, and they are zeroed when the last handle is gone. Go's `ssh.Signer` over `ssh.ParseRawPrivateKey`, with the
writing that Go does not have.

## Rules

- The key's bytes never pass through managed memory: a file is read into a `crypto::secret_bytes`
  ([crypto::read_secret](../../../crypto/read_secret.md)) and decoded in unmanaged memory, and what
  [to_openssh](to_openssh.md) gives is a `secret_bytes`.
- An RSA key signs with rsa-sha2-512 (or rsa-sha2-256 where a server asks for it), never SHA-1.
- RSA keys of 1024 bits and more are read; [generate](generate.md) makes 3072 by default, as ssh-keygen does.
- A key file's errors are crypto's codes: `malformed` for a text that is no key of these forms,
  `unsupported` for a cipher, a KDF or a key type not read here (DSA, security keys, ECDSA with explicit curve
  parameters), `authentication` for an encrypted key with no passphrase or a wrong one.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](private_key.md) | no key |
| [parse](parse.md) | a key from a file's text (static) |
| [load](load.md) | a key from a file (static) |
| [generate](generate.md) | a new key (static) |
| [public_key](public_key.md) | the public half |
| [type](type.md) | the kind of key |
| [comment](comment.md) | the key's comment |
| [with_comment](with_comment.md) | the key with another comment |
| [to_openssh](to_openssh.md) | the key in OpenSSH's file format |
| [save](save.md) | the key written to a file |
| [sign](sign.md) | the signature of data |
| [operator bool](operator_bool.md) | whether there is a key |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::private_key key = net::ssh::private_key::load("tests/net/ssh/testdata/ed25519");
    println("{}", key.public_key().fingerprint());
    net::ssh::private_key locked = net::ssh::private_key::load("tests/net/ssh/testdata/ed25519_enc", "correct horse");
    println("{}", locked.comment());
    auto wrong = net::ssh::private_key::load("tests/net/ssh/testdata/ed25519_enc", "wrong");
    println("{}", wrong.error().code() == crypto::errc::authentication);
}
```

Output:

```text
SHA256:PN89yHvZV5qRnQP3eclDnzJi7J8IfYfMitxf9jaYMn0
test-ed25519-enc
true
```

## See also

- [public_key](../public_key/README.md)
- [client::options](../client-options.md): `keys`; [server](../server/README.md): `host_keys`
- [agent](../agent/README.md): keys held by an agent
- [net::ssh](../README.md)
