[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [authorized_keys](README.md)

# sgcl::net::ssh::authorized_keys::load

```cpp
static expected<authorized_keys, io::error> load(const string& path) noexcept;
```

The lines of a file (a user's `~/.ssh/authorized_keys`); lines that cannot be read (comments, a key of a kind not read here, broken options) are passed over, as sshd passes them.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

The set. Or the [io::error](../../../io/error/README.md) of the file.

## Complexity

Linear in the file.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/p256.pub"));
    io::write_file("authorized_keys", "no-pty " + key.to_string() + "\n");
    net::ssh::authorized_keys keys = net::ssh::authorized_keys::load("authorized_keys");
    println("{} {}", keys.size(), keys.find(key)->has("no-pty"));
}
```

Output:

```text
1 true
```

## See also

- [parse](parse.md)
- [sgcl::net::ssh::authorized_keys](README.md)
