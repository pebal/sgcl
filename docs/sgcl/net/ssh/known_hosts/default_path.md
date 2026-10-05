[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [known_hosts](README.md)

# sgcl::net::ssh::known_hosts::default_path

```cpp
static string default_path() noexcept;
```

The user's file: `$HOME/.ssh/known_hosts`, what [load](load.md) reads by default and what ssh reads.

## Parameters

None.

## Return value

The path.

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
    println("{}", net::ssh::known_hosts::default_path().ends_with("/.ssh/known_hosts"));
}
```

Output:

```text
true
```

## See also

- [load](load.md)
- [sgcl::net::ssh::known_hosts](README.md)
