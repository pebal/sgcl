[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [authorized_keys](../authorized_keys/README.md)

# sgcl::net::ssh::authorized_keys::entry

```cpp
#include "sgcl/net/ssh/authorized_keys.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    class authorized_keys {
    public:
        struct entry {
            ssh::public_key key;
            vector<pair<string, string>> options;

            bool has(std::string_view name) const noexcept;
            string option(std::string_view name) const noexcept;
        };
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::authorized_keys::entry` is a line of authorized_keys as [find](../authorized_keys/find.md) gives it: its key
and its options in their order, names and values (`command="uptime"` is `{"command", "uptime"}`, `no-pty` is
`{"no-pty", ""}`), the quotes taken off and `\"` read as `"`. A plain struct with two helpers.

## Member objects

| Member | Description |
|---|---|
| `key` | the line's key (an authority's, on a `cert-authority` line) |
| `options` | its options: names and values in their order |

## Member functions

| Function | Description |
|---|---|
| [has](has.md) | whether the line has an option |
| [option](option.md) | an option's value |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/p256.pub"));
    net::ssh::authorized_keys keys =
        net::ssh::authorized_keys::parse("from=\"10.0.0.0/8\",command=\"echo \\\"hi\\\"\",no-pty " + key.to_string());
    net::ssh::authorized_keys::entry line = keys.find(key).value();
    for (auto& [name, value] : line.options) {
        println("{} = {}", name, value);
    }
}
```

Output:

```text
from = 10.0.0.0/8
command = echo "hi"
no-pty = 
```

## See also

- [authorized_keys::find](../authorized_keys/find.md)
- [sgcl::net::ssh::authorized_keys](../authorized_keys/README.md)
