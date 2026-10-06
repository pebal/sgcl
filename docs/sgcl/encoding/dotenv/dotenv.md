[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::dotenv::dotenv

```cpp
dotenv() noexcept;
```

No entries; [from](from.md) makes some, [parse](parse.md) and [load](load.md) read them. The copy and the move are
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
    encoding::dotenv env;
    println("{} {}", env.size(), env.empty());
    println(env.set("A", "1").size());
}
```

Output:

```text
0 true
1
```

## See also

- [from](from.md)
- [sgcl::encoding::dotenv](README.md)
