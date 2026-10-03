[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](../sorted_multiset.md)

# sgcl::sorted_multiset\<Key, Compare\>::rend, crend

```cpp
reverse_iterator rend() noexcept;                 // (1)
const_reverse_iterator rend() const noexcept;     // (2)
const_reverse_iterator crend() const noexcept;    // (3)
```

Returns the reverse iterator past the smallest element, the end of the order read backwards: `begin()` wrapped in
a `std::reverse_iterator`. It may not be dereferenced.

## Parameters

None.

## Return value

The reverse iterator past the first element.

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
    sorted_multiset<char> letters = {'c', 'a', 'b', 'a'};

    vector<char> backwards;
    for (auto it = letters.rbegin(); it != letters.rend(); ++it) {
        backwards.push_back(*it);
    }
    println("{}", backwards);
    println("{}", letters.crbegin() == letters.crend());
}
```

Output:

```text
['c', 'b', 'a', 'a']
false
```

## See also

- [rbegin, crbegin](rbegin.md): the order from the largest element
- [end, cend](end.md): the iterator past the last element
- [sgcl::sorted_multiset\<Key, Compare\>](../sorted_multiset.md)
