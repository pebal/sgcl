[sgcl](../../README.md) › [immutable](../README.md) › [vector](README.md)

# sgcl::immutable::vector\<T\>::begin, cbegin

```cpp
const_iterator begin() const noexcept;     // (1)
const_iterator cbegin() const noexcept;    // (2)
```

Returns an iterator to the first element; on an empty vector it is equal to [end()](end.md).

- (1–2) The same iterator: every iterator of the vector is a `const_iterator`.

The iterator is random-access, so the algorithms of `<algorithm>` and `std::ranges` apply. It holds a pointer to
this vector object and the position, and remembers the leaf it is in: a walk looks the trie up once per 32
elements, not once per element.

## Parameters

None.

## Return value

An iterator to the first element.

## Complexity

Constant.

## Exceptions

None.

## Notes

A walk of a hundred thousand elements costs 0.55 ns per element. The iterator keeps nothing alive: it is valid
while this vector object exists and holds the version it was taken from; a version copied elsewhere is another
object with iterators of its own.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"
#include <algorithm>
#include <numeric>

using namespace sgcl;

int main() {
    immutable::vector<int> v = {5, 3, 9, 1};
    auto smallest = std::ranges::min_element(v);
    int sum = std::accumulate(v.begin(), v.end(), 0);
    println("{} at {}, sum {}", *smallest, smallest - v.begin(), sum);

    for (auto it = v.cbegin(); it != v.cend(); it += 2) {
        println("{}", *it);
    }
}
```

Output:

```text
1 at 3, sum 18
5
9
```

## See also

- [end, cend](end.md): an iterator to the end
- [rbegin, crbegin](rbegin.md): a reverse iterator to the beginning
- [sgcl::immutable::vector\<T\>](README.md)
