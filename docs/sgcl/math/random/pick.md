[sgcl](../../README.md) › [math](../README.md) › [random](../random.md)

# sgcl::math::random::pick

```cpp
template<std::ranges::random_access_range R>
requires std::is_lvalue_reference_v<std::ranges::range_reference_t<R>>
decltype(auto) pick(R& range);                                            // (1)
template<class R>
void pick(const R&&) = delete;                                            // (2)
```

1. One element of `range`, each as likely. The element itself, not a copy: `r.pick(cards) = 0` writes into
   `cards`. The range has to outlive the call, and its elements have to be objects a reference can be taken to,
   which is why a temporary is not taken and neither is a view that makes its elements as it goes.
2. Deleted: a `const` temporary would otherwise bind to (1), `R` deduced `const`, and leave the reference hanging.
   A non-`const` temporary does not bind to (1) at all.

## Parameters

| Parameter | Description |
|---|---|
| `range` | the range an element is picked from, not empty |

## Return value

A reference to the element picked.

## Complexity

Constant: a draw, rarely two.

## Exceptions

`out_of_range` when `range` is empty.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::random r(42);
    vector<string> colors = {"red", "green", "blue"};
    println("{} {} {}", r.pick(colors), r.pick(colors), r.pick(colors));

    vector seats = {1, 2, 3, 4};
    r.pick(seats) = 0;  // the element itself
    println("{}", seats);

    vector<int> none;
    try {
        r.pick(none);
    } catch (const out_of_range& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
blue red red
[1, 0, 3, 4]
sgcl::math::random::pick: an empty range
```

## See also

- [next_int](next_int.md): a position drawn the same way
- [shuffle](shuffle.md): every element, in a random order
- [sgcl::math::random](../random.md)
