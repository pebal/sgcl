[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [authorized_keys](../authorized_keys/README.md) › [entry](README.md)

# sgcl::net::ssh::authorized_keys::entry::has

```cpp
bool has(std::string_view name) const noexcept;
```

Whether the line has the option `name`, with a value or without.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the option's name |

## Return value

Whether it has it.

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
    println("{} {}", keys.find(key)->has("no-pty"), keys.find(key)->has("restrict"));
}
```

Output:

```text
true false
```

## See also

- [sgcl::net::ssh::authorized_keys::entry](README.md)
