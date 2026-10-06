[sgcl](../../README.md) › [encoding](../README.md) › [ini](README.md)

# sgcl::encoding::ini::ini

```cpp
ini() noexcept;
```

No sections; [set](set.md) adds some, [parse](parse.md) and [load](load.md) read them. The copy and the move are
the implicit ones and copy the handle.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::ini config;
    println("{} {}", config.size(), config.empty());
    print(config.set("server", "port", "8080").to_string());
}
```

Output:

```text
0 true
[server]
port = 8080
```

## See also

- [set](set.md)
- [sgcl::encoding::ini](README.md)
