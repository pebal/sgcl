[sgcl](../../README.md) › [concurrent](../README.md) › [error](README.md)

# sgcl::concurrent::error::message

```cpp
string message() const noexcept;
```

Returns the sentence that says why the bytes do not read.

## Parameters

None.

## Return value

The sentence.

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
    vector<byte> nothing;
    println("{}", concurrent::bloom_filter::from_bytes(nothing).error().message());
}
```

Output:

```text
too short for the sketch's header
```

## See also

- [offset](offset.md)
- [sgcl::concurrent::error](README.md)
