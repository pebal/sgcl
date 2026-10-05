[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [known_hosts](README.md)

# sgcl::net::ssh::known_hosts::check

```cpp
expected<void, io::error> check(const string& address, const ssh::public_key& key) const noexcept;
```

Whether `key` is the host's at `address`, `"host:port"` or `"host"` (port 22): a plain key known by a line of the
host's names, or a host certificate whose authority a `@cert-authority` line of the host's names holds, valid now,
of the host type, naming the host among its principals.

## Parameters

| Parameter | Description |
|---|---|
| `address` | the host, `"host:port"` or `"host"` |
| `key` | the key it presented |

## Return value

Nothing when it is known. Or the [io::error](../../../io/error/README.md), op `ssh known_hosts`:
`net::errc::ssh_host_key_revoked` for a key or an authority a `@revoked` line names, `net::errc::ssh_host_key_mismatch`
when the host is known with another key of the same type (the warning ssh shouts about), `net::errc::ssh_host_key_unknown`
otherwise, `net::errc::invalid_address` for an address that is neither form.

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
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519.pub"));
    net::ssh::public_key other = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519_enc.pub"));
    net::ssh::known_hosts hosts = net::ssh::known_hosts::parse("[example.com]:2222 " + key.to_string() +
                                                               "\n@revoked old.example.com " + other.to_string());
    println("{}", hosts.check("example.com:2222", key).has_value());
    println("{}", hosts.check("example.com:2222", other).error().code() == net::errc::ssh_host_key_mismatch);
    println("{}", hosts.check("example.com", key).error().code() == net::errc::ssh_host_key_unknown);
    println("{}", hosts.check("old.example.com", other).error().code() == net::errc::ssh_host_key_revoked);
}
```

Output:

```text
true
true
true
true
```

## See also

- [add](add.md)
- [client::options](../client-options.md)
- [sgcl::net::ssh::known_hosts](README.md)
