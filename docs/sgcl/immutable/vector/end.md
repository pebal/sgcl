[sgcl](../../README.md) › [immutable](../README.md) › [vector](../vector.md)

# sgcl::immutable::vector\<T\>::end, cend

```cpp
/*(1)*/ const_iterator end() const noexcept;
/*(2)*/ const_iterator cend() const noexcept;
```

Returns an iterator past the last element. It may not be dereferenced.

- (1–2) The same iterator: every iterator of the vector is a `const_iterator`.

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
    immutable::vector<int> v = {1, 2, 3};
    println("{} elements", v.end() - v.begin());
    println("{}", *(v.cend() - 1));
}
```

Output:

```text
3 elements
3
```

## See also

- [begin, cbegin](begin.md): an iterator to the beginning
- [rend, crend](rend.md): a reverse iterator to the end
- [sgcl::immutable::vector\<T\>](../vector.md)
