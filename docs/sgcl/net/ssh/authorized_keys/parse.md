[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [authorized_keys](README.md)

# sgcl::net::ssh::authorized_keys::parse

```cpp
static authorized_keys parse(const string& text) noexcept;
```

The lines of a text in the file's format; lines that cannot be read are passed over.

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
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/p256.pub"));
    net::ssh::authorized_keys keys = net::ssh::authorized_keys::parse("# comment\ncommand=\"uptime\" " + key.to_string());
    println("{}", keys.find(key)->option("command"));
}
```

Output:

```text
uptime
```

## See also

- [entry](../authorized_keys-entry/README.md)
- [sgcl::net::ssh::authorized_keys](README.md)
