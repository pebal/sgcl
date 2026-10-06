[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::dotenv::size

```cpp
size_t size() const noexcept;
```

The entries; a key given twice counted once.

## Parameters

None.

## Return value

The count.

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
    println(encoding::dotenv::parse("A=1\nB=2\nA=3")->size());
}
```

Output:

```text
2
```

## See also

- [empty](empty.md)
- [sgcl::encoding::dotenv](README.md)
