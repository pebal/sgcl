[sgcl](../../README.md) › [math](../README.md) › [random](../random.md)

# sgcl::math::random::shuffle

```cpp
template<std::ranges::random_access_range R>
void shuffle(R&& range)
    noexcept(noexcept(std::ranges::begin(range)) && noexcept(std::ranges::distance(range))
             && noexcept(std::ranges::iter_swap(std::ranges::begin(range)
                                                + std::ranges::distance(range),
                                                std::ranges::begin(range))));
```

Puts the elements of `range` in a random order, every order as likely: Fisher and Yates from the back, as Go's
`Shuffle`, so with Go's stream the same order. Where Go's `Shuffle(n, swap)` takes a count and a function that
swaps two elements, this takes the range itself: any range of random access, a [vector](../../core/vector.md), an
[array](../../core/array.md), a `std::vector`, a built-in array, a part of one.

## Parameters

| Parameter | Description |
|---|---|
| `range` | the elements shuffled in place |

## Return value

None.

## Complexity

Linear in the size of `range`: a draw and a swap for every element but the first.

## Exceptions

None when beginning, measuring and swapping the elements of the range cannot throw, which is so for the
containers of the library and of `std`; otherwise what they throw.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::random r(42);
    vector cards = {1, 2, 3, 4, 5, 6, 7, 8};
    r.shuffle(cards);
    println("{}", cards);

    vector<string> names = {"ada", "brian", "grace", "ken"};
    r.shuffle(names);
    println("{}", names);
}
```

Output:

```text
[4, 8, 1, 6, 5, 7, 2, 3]
["grace", "brian", "ada", "ken"]
```

## See also

- [permutation](permutation.md): the shuffled indices `0 … n - 1`
- [pick](pick.md): one element
- [sgcl::math::random](../random.md)
