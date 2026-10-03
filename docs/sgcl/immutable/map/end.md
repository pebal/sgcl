[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::end, cend

```cpp
const_iterator end() const noexcept;     // (1)
const_iterator cend() const noexcept;    // (2)
```

Returns an iterator past the last element, equal to a default-constructed `const_iterator`; it is also what
[find](find.md) returns for a key that is absent. It may not be dereferenced.

- (1–2) The same iterator: every iterator of the map is a `const_iterator`.

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
    immutable::map<int, string> names = {{1, "one"}, {2, "two"}};
    println("{} {}", names.find(3) == names.end(), names.find(2) == names.cend());
}
```

Output:

```text
true false
```

## See also

- [begin, cbegin](begin.md): an iterator to the beginning
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](../map.md)
