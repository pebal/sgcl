[sgcl](../../README.md) › [immutable](../README.md) › [set](../set.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::end, cend

```cpp
/*(1)*/ const_iterator end() const noexcept;
/*(2)*/ const_iterator cend() const noexcept;
```

Returns an iterator past the last element, equal to a default-constructed `const_iterator`; it is also what
[find](find.md) returns for a key that is absent. It may not be dereferenced.

- (1–2) The same iterator: every iterator of the set is a `const_iterator`.

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
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::set<int> s = {1, 2};
    println("{} {}", s.find(3) == s.end(), s.find(2) == s.cend());
}
```

Output:

```text
true false
```

## See also

- [begin, cbegin](begin.md): an iterator to the beginning
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](../set.md)
