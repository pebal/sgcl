[sgcl](../../README.md) › [core](../README.md) › [vector](../vector.md)

# sgcl::vector\<T\>::back

```cpp
/*(1)*/ reference back() noexcept;
/*(2)*/ const_reference back() const noexcept;
```

Returns a reference to the last element, `(*this)[size() - 1]`. The vector must not be empty.

## Parameters

None.

## Return value

A reference to the last element.

## Complexity

Constant.

## Exceptions

None.

## Notes

`back` on an empty vector is undefined behaviour, as with `std::vector`. The reference does not keep the buffer
alive: it is valid until the element is removed, the vector reallocates or the vector is destroyed.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<int> pile;
    for (int i : range(1, 4)) {
        pile.push_back(i * 10);
    }
    while (!pile.empty()) {
        println("{}", pile.back());
        pile.pop_back();
    }
}
```

Output:

```text
30
20
10
```

## See also

- [front](front.md): access the first element
- [push_back](push_back.md), [pop_back](pop_back.md): append, remove the last element
- [sgcl::vector\<T\>](../vector.md)
