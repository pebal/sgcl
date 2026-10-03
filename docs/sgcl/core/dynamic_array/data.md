[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](../dynamic_array.md)

# sgcl::dynamic_array\<T\>::data

```cpp
T* data() noexcept;                // (1)
const T* data() const noexcept;    // (2)
```

Returns a pointer to the first element of the buffer: the elements lie in one block, and
`[data(), data() + size())` is a valid range. When the array holds no buffer — default-constructed, made with no
elements, moved from — it is `nullptr`.

## Parameters

None.

## Return value

A pointer to the first element, or `nullptr` when there is no buffer.

## Complexity

Constant.

## Exceptions

None.

## Notes

The buffer never moves, so the pointer stays the same for as long as the array holds that buffer, until it is
assigned over, moved from or destroyed. The pointer does not keep the buffer alive; a [slice](../slice.md) from
[as_slice](as_slice.md) does.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>
#include <cstring>
#include <string_view>

using namespace sgcl;

int main() {
    dynamic_array<char> text(6, '\0');
    std::ranges::copy(std::string_view("hello"), text.data());
    println("{} {}", text.data(), std::strlen(text.data()));

    dynamic_array<int> none;
    println("{}", none.data() == nullptr);
}
```

Output:

```text
hello 5
true
```

## See also

- [as_slice, operator slice](as_slice.md): the elements as a slice that holds the buffer
- [begin, cbegin](begin.md): an iterator to the first element
- [sgcl::dynamic_array\<T\>](../dynamic_array.md)
