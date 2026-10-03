[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](README.md)

# sgcl::dynamic_array\<T\>::rend, crend

```cpp
reverse_iterator rend() noexcept;                 // (1)
const_reverse_iterator rend() const noexcept;     // (2)
const_reverse_iterator crend() const noexcept;    // (3)
```

Returns a reverse iterator past the first element, the end of the walk from the back:
`reverse_iterator(begin())`. It may not be dereferenced.

- (1) The end of the reverse iterators that write the elements.
- (2–3) The end of the reverse iterators that read them.

## Parameters

None.

## Return value

A reverse iterator past the first element.

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
    dynamic_array<char> word = {'l', 'e', 'v', 'e', 'r'};
    dynamic_array<char> backwards(word.rbegin(), word.rend());
    println("{}", backwards);
}
```

Output:

```text
['r', 'e', 'v', 'e', 'l']
```

## See also

- [rbegin, crbegin](rbegin.md): a reverse iterator to the beginning
- [end, cend](end.md): an iterator to the end
- [sgcl::dynamic_array\<T\>](README.md)
