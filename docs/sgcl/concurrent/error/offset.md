[sgcl](../../README.md) › [concurrent](../README.md) › [error](README.md)

# sgcl::concurrent::error::offset

```cpp
size_t offset() const noexcept;
```

Returns the byte of the input the reading stopped on: where the field that failed starts, or the size of the input for one too short or too long.

## Parameters

None.

## Return value

The byte.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> b = concurrent::bloom_filter(10).to_bytes();
    b[5] = byte(0);  // hashes: zero
    println("{}", concurrent::bloom_filter::from_bytes(b).error().offset());
}
```

Output:

```text
5
```

## See also

- [message](message.md)
- [sgcl::concurrent::error](README.md)
