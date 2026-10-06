[sgcl](../../README.md) › [concurrent](../README.md) › [error](README.md)

# sgcl::concurrent::error::error

```cpp
explicit error(const string& message, size_t offset = 0) noexcept;
```

Constructs the error of a sentence and the byte of the input it stopped on: what the library returns, and what a test
or a function of the program's own that passes the module's failures on makes.

## Parameters

| Parameter | Description |
|---|---|
| `message` | the sentence |
| `offset` | the byte of the input; 0 by default |

## Complexity

Constant: the string is shared.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::error e("not a sketch", 3);
    println("{} at {}", e.message(), e.offset());
}
```

Output:

```text
not a sketch at 3
```

## See also

- [message](message.md), [offset](offset.md)
- [sgcl::concurrent::error](README.md)
