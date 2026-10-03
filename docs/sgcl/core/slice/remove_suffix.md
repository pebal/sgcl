[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::remove_suffix

```cpp
void remove_suffix(size_type n) noexcept;
```

Narrows the slice in place by its last `n` elements, as `std::string_view::remove_suffix`; the owner stays.
Precondition: `n <= size()`; a debug build asserts it.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of elements to drop from the end |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string line = "hello\r\n";
    string_slice text = line;
    text.remove_suffix(2);
    println("[{}]", text);
}
```

Output:

```text
[hello]
```

## See also

- [remove_prefix](remove_prefix.md): narrows the slice from the start
- [sgcl::slice\<T\>](README.md)
