[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [authorized_keys](../authorized_keys/README.md) › [entry](README.md)

# sgcl::net::ssh::authorized_keys::entry::option

```cpp
string option(std::string_view name) const noexcept;
```

The value of the option `name`: its text without the quotes, empty for an option without a value or one the line does not have ([has](has.md) tells them apart).

## Parameters

| Parameter | Description |
|---|---|
| `name` | the option's name |

## Return value

The value.

## Complexity

Linear in the options.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/p256.pub"));
    net::ssh::authorized_keys keys = net::ssh::authorized_keys::parse("command=\"uptime\",no-pty " + key.to_string());
    println("[{}] [{}]", keys.find(key)->option("command"), keys.find(key)->option("from"));
}
```

Output:

```text
[uptime] []
```

## See also

- [sgcl::net::ssh::authorized_keys::entry](README.md)
