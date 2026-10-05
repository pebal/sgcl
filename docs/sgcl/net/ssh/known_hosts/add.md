[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [known_hosts](README.md)

# sgcl::net::ssh::known_hosts::add

```cpp
expected<void, io::error> add(const string& address, const ssh::public_key& key, bool hashed = false) const noexcept;
```

A line for the host at `address` and its key added to the set, and appended to the file the set was loaded from (made,
with mode 0600, when it is not there): what ssh does when the user accepts a new host. Hashed, the host's name is
written as `ssh-keygen -H` writes it (`|1|salt|HMAC-SHA1`), so that the file does not list the hosts.

## Parameters

| Parameter | Description |
|---|---|
| `address` | the host, `"host:port"` or `"host"` |
| `key` | its key |
| `hashed` | the name written hashed |

## Return value

Nothing; or the [io::error](../../../io/error/README.md) of the file, `net::errc::invalid_address` for an address that is neither form.

## Complexity

Linear in the line.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519.pub"));
    net::ssh::public_key other = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519_enc.pub"));
    net::ssh::known_hosts hosts = net::ssh::known_hosts::load("known_hosts");
    hosts.add("example.com", key);
    hosts.add("secret.example.com", other, true);
    print("{}", io::read_text("known_hosts")->substr(0, 12));
    println("{}", net::ssh::known_hosts::load("known_hosts")->check("secret.example.com", other).has_value());
}
```

Output:

```text
example.com true
```

## See also

- [check](check.md)
- [to_string](to_string.md)
- [sgcl::net::ssh::known_hosts](README.md)
