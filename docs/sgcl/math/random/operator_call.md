[sgcl](../../README.md) › [math](../README.md) › [random](README.md)

# sgcl::math::random::operator()

```cpp
result_type operator()() noexcept;
```

The next word of the stream, as [next_uint64](next_uint64.md). With `result_type`, [min](min.md) and [max](max.md)
it makes a `random` a uniform random bit generator of the standard library's
(`std::uniform_random_bit_generator`), so every distribution of `<random>`, `std::shuffle` and
`std::ranges::sample` take a `random` as they take `mt19937_64`.

## Parameters

None.

## Return value

The word drawn, a `uint64_t`.

## Complexity

Constant: one draw.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"
#include <algorithm>
#include <random>

using namespace sgcl;

int main() {
    math::random r(42);
    println("{}", std::uniform_random_bit_generator<math::random>);

    std::poisson_distribution<int> arrivals(3.5);
    long total = 0;
    for (int i : range(100000)) {
        total += arrivals(r);
    }
    println("{:.1f} arrivals on the average", total / 100000.0);

    vector letters = {'a', 'b', 'c', 'd', 'e', 'f'};
    vector<char> chosen(3);
    std::ranges::sample(letters, chosen.begin(), 3, r);
    println("{} in order: {}", chosen.size(), std::ranges::is_sorted(chosen));
}
```

Output:

```text
true
3.5 arrivals on the average
3 in order: true
```

## See also

- [next_uint64](next_uint64.md): the same word under its own name
- [min](min.md), [max](max.md): the range of a draw
- [sgcl::math::random](README.md)
