[sgcl](../../README.md) › [math](../README.md) › [fft](README.md)

# sgcl::math::fft::size

```cpp
size_t size() const noexcept;
```

The length of the transforms the plan makes: the `n` it was made with.

## Parameters

None.

## Return value

The length.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {}", math::fft(48).size(), math::fft(0).size());
}
```

Output:

```text
48 0
```

## See also

- [(constructor)](fft.md): the plan of a length
- [sgcl::math::fft](README.md)
