[sgcl](../../README.md) › [core](../README.md) › [slice](../slice.md)

# sgcl::slice\<T\>::remove_prefix

```cpp
void remove_prefix(size_type n) noexcept;
```

Narrows the slice in place by its first `n` elements, as `std::string_view::remove_prefix`; the owner stays.
Precondition: `n <= size()`; a debug build asserts it.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of elements to drop from the start |

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
    string line = "GET /index.html";
    string_slice rest = line;
    rest.remove_prefix(rest.find(' ') + 1);
    println("{}", rest);
}
```

Output:

```text
/index.html
```

## See also

- [remove_suffix](remove_suffix.md): narrows the slice from the end
- [trim_prefix](trim_prefix.md): a new slice without a prefix, when it is there
- [sgcl::slice\<T\>](../slice.md)
