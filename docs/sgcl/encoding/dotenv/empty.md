[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::dotenv::empty

```cpp
bool empty() const noexcept;
```

Whether there is no entry.

## Parameters

None.

## Return value

`true` when there is none.

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
    println("{} {}", encoding::dotenv::parse("# nothing but a comment\n")->empty(), encoding::dotenv::parse("A=1")->empty());
}
```

Output:

```text
true false
```

## See also

- [size](size.md)
- [sgcl::encoding::dotenv](README.md)
