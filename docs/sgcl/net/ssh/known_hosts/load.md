[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [known_hosts](README.md)

# sgcl::net::ssh::known_hosts::load

```cpp
static expected<known_hosts, io::error> load(const string& path = default_path()) noexcept;
```

The set of a file, by default the user's `~/.ssh/known_hosts`; a file that is not there is an empty set, which [add](add.md) creates. Lines that cannot be read are passed over.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

The set. Or the [io::error](../../../io/error/README.md) of the file (`EACCES` …), but `ENOENT`.

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
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519.pub"));
    net::ssh::public_key other = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519_enc.pub"));
    net::ssh::known_hosts hosts = net::ssh::known_hosts::load("known_hosts");  // not there yet
    hosts.add("example.com:2222", key);
    net::ssh::known_hosts again = net::ssh::known_hosts::load("known_hosts");
    println("{} {}", again.size(), again.check("example.com:2222", key).has_value());
}
```

Output:

```text
1 true
```

## See also

- [add](add.md), [default_path](default_path.md)
- [sgcl::net::ssh::known_hosts](README.md)
