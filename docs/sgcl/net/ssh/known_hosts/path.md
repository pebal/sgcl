[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [known_hosts](README.md)

# sgcl::net::ssh::known_hosts::path

```cpp
string path() const noexcept;
```

The file [add](add.md) appends to: the one the set was loaded from; empty for a set parsed from text or made empty.

## Parameters

None.

## Return value

The path, or an empty string.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    println("[{}] [{}]", net::ssh::known_hosts::load("hosts")->path(), net::ssh::known_hosts().path());
}
```

Output:

```text
[hosts] []
```

## See also

- [load](load.md)
- [sgcl::net::ssh::known_hosts](README.md)
