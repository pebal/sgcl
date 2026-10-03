[sgcl](../../README.md) › [core](../README.md) › [array](README.md)

# sgcl::array\<T, N\>::begin, cbegin

```cpp
constexpr iterator begin() noexcept;                 // (1)
constexpr const_iterator begin() const noexcept;     // (2)
constexpr const_iterator cbegin() const noexcept;    // (3)
```

Returns an iterator to the first element; for `array<T, 0>` it is equal to [end()](end.md).

- (1) An iterator that writes the elements.
- (2–3) An iterator that reads them.

The iterator is a raw pointer in a thin class, a `std::contiguous_iterator`: cheap to copy, at home in any
container, and the algorithms of `<algorithm>` and `std::ranges` apply.

## Parameters

None.

## Return value

An iterator to the first element.

## Complexity

Constant.

## Exceptions

None.

## Notes

An iterator keeps nothing alive. One that dies in a frame nulls its word, so that a temporary left behind does
not count as a root under the collector's conservative scan of the stack. An iterator is `constexpr`, as the
array's members are, so a walk runs in a constant expression too.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>
#include <numeric>

using namespace sgcl;

constexpr int total(const array<int, 4>& a) {
    int sum = 0;
    for (auto it = a.cbegin(); it != a.cend(); ++it) {
        sum += *it;
    }
    return sum;
}

int main() {
    array<int, 4> a = {5, 3, 9, 1};
    std::ranges::sort(a.begin(), a.end());
    *a.begin() = 0;
    println("{} {}", a, std::accumulate(a.begin(), a.end(), 0));

    constexpr int sum = total({1, 2, 3, 4});
    println("{}", sum);
}
```

Output:

```text
[0, 3, 5, 9] 17
10
```

## See also

- [end, cend](end.md): an iterator to the end
- [rbegin, crbegin](rbegin.md): a reverse iterator to the beginning
- [sgcl::array\<T, N\>](README.md)
