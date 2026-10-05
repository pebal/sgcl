[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [private_key](README.md)

# sgcl::net::ssh::private_key::save

```cpp
expected<void, io::error> save(const string& path, const string& passphrase = {}) const noexcept;
```

[to_openssh](to_openssh.md) written to a file made with mode 0600 (replaced, and its mode set, when it is there), as ssh-keygen writes `~/.ssh/id_ed25519`.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |
| `passphrase` | the passphrase to encrypt with; empty: unencrypted |

## Return value

Nothing; or the [io::error](../../../io/error/README.md) of the file.

## Complexity

Linear in the key, as [to_openssh](to_openssh.md).

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::private_key key = net::ssh::private_key::generate().with_comment("me@here");
    key.save("id_ed25519");
    println("{:o}", unsigned(io::stat("id_ed25519")->mode) & 0777);
    println("{}", net::ssh::private_key::load("id_ed25519")->comment());
}
```

Output:

```text
600
me@here
```

## See also

- [load](load.md)
- [to_openssh](to_openssh.md)
- [sgcl::net::ssh::private_key](README.md)
