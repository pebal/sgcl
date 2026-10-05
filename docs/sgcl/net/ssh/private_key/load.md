[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [private_key](README.md)

# sgcl::net::ssh::private_key::load

```cpp
static expected<private_key, io::error> load(const string& path, const string& passphrase = {}) noexcept;
```

The key of a file, as [parse](parse.md) reads a text: the file's bytes read straight into a `crypto::secret_bytes`,
never into managed memory. What `ssh` reads from `~/.ssh/id_ed25519`.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the key file |
| `passphrase` | the passphrase of an encrypted key; empty: none |

## Return value

The key. Or the [io::error](../../../io/error/README.md) of the file (`ENOENT` …), or [parse](parse.md)'s, its path the
file's.

## Complexity

Linear in the file, as [parse](parse.md).

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::private_key key = net::ssh::private_key::load("tests/net/ssh/testdata/rsa_enc", "correct horse");
    println("{} {}", key.public_key().type_name(), key.comment());
    println("{}", net::ssh::private_key::load("no/such/key").error().is_not_found());
}
```

Output:

```text
ssh-rsa test-rsa-enc
true
```

## See also

- [parse](parse.md)
- [save](save.md)
- [sgcl::net::ssh::private_key](README.md)
