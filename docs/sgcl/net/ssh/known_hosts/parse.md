[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [known_hosts](README.md)

# sgcl::net::ssh::known_hosts::parse

```cpp
static known_hosts parse(const string& text) noexcept;
```

The set of a text in the file's format, of no file: [add](add.md) adds to the set alone. Lines that cannot be read are passed over.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the lines |

## Return value

The set.

## Complexity

Linear in the text.

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
    net::ssh::known_hosts hosts = net::ssh::known_hosts::parse("# a comment\nbroken line\nexample.com " + key.to_string());
    println("{} {}", hosts.size(), hosts.path().empty());
}
```

Output:

```text
1 true
```

## See also

- [load](load.md)
- [sgcl::net::ssh::known_hosts](README.md)
