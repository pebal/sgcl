[sgcl](../../README.md) › [core](../README.md) › [vector](../vector.md)

# sgcl::vector\<T\>::end, cend

```cpp
/*(1)*/ iterator end() noexcept;
/*(2)*/ const_iterator end() const noexcept;
/*(3)*/ const_iterator cend() const noexcept;
```

Returns an iterator past the last element, `begin() + size()`. It addresses no element and must not be
dereferenced.

- (1) An `iterator`.
- (2–3) A `const_iterator`: `cend` gives it on a vector that is not `const` too.

## Parameters

None.

## Return value

An iterator past the last element.

## Complexity

Constant.

## Exceptions

None.

## Notes

`end()` changes with the size: an append, an insertion or a removal invalidates it
([Iterator invalidation](../vector.md#iterator-invalidation)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<string> words = {"one", "two", "three"};
    for (auto it = words.cbegin(); it != words.cend(); ++it) {
        println("{}", *it);
    }
    println("{}", words.end() - words.begin());
}
```

Output:

```text
one
two
three
3
```

## See also

- [begin, cbegin](begin.md): an iterator to the beginning
- [rend, crend](rend.md): a reverse iterator to the end
- [sgcl::vector\<T\>](../vector.md)
