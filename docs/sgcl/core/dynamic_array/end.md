[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](../dynamic_array.md)

# sgcl::dynamic_array\<T\>::end, cend

```cpp
iterator end() noexcept;                 // (1)
const_iterator end() const noexcept;     // (2)
const_iterator cend() const noexcept;    // (3)
```

Returns an iterator past the last element, `begin() + size()`. It may not be dereferenced.

- (1) The end of the iterators that write the elements.
- (2–3) The end of the iterators that read them.

## Parameters

None.

## Return value

An iterator past the last element.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>

using namespace sgcl;

int main() {
    dynamic_array<int> a = {2, 4, 6, 7, 8};
    auto odd = std::find_if(a.cbegin(), a.cend(), [](int x) { return x % 2 != 0; });
    println("{} at {}", *odd, odd - a.cbegin());

    dynamic_array<int> none;
    println("{}", none.begin() == none.end());
}
```

Output:

```text
7 at 3
true
```

## See also

- [begin, cbegin](begin.md): an iterator to the beginning
- [rend, crend](rend.md): a reverse iterator to the end
- [sgcl::dynamic_array\<T\>](../dynamic_array.md)
