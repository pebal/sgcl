[sgcl](../README.md) › [async](README.md)

# sgcl::async::parallel_reduce

```cpp
#include "sgcl/async/parallel.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class T, class R, class Map>
    R parallel_reduce(T begin, T end, R init, Map map, const parallel_options& options = {});    // (1)
    template<class T, class R, class Map, class Combine>
    R parallel_reduce(T begin, T end, R init, Map map, Combine combine,                          // (2)
                      const parallel_options& options = {});
}
```

Maps every index of `begin .. end - 1` to a value and combines the values with `init`, the work spread over the
workers of the [scheduler](scheduler.md) and the caller as [parallel_for](parallel_for.md) spreads it:
`std::transform_reduce(std::execution::par, ...)` over a range of integers, with the map before the combine. The
work runs inside the call, which returns the result, on a thread and in a task alike.

1. Combines with `a + b`.
2. Combines with `combine(a, b)`.

The result is the left fold in the order of the indices,
`combine(...combine(combine(init, map(begin)), map(begin + 1))..., map(end - 1))`, regrouped: each chunk of
consecutive indices is folded from its first index into a partial result of its own, on the lane that claimed it,
and the caller folds the partials into `init` in the order of the chunks at the end. `combine` meets neighbours only,
the left one first, and `init` once, on the far left. So the result is the sequential fold's for any `combine` that
is associative; it need not be commutative (a concatenation is in the order of the indices). A floating-point sum
is regrouped as well, and may differ from the sequential sum in its last bits, by the grain and the lanes.

`map(i)` is called with the index, or `map(i, lane)` when it takes two arguments, the lane as
[parallel_for](parallel_for.md) gives it. `T` is an integer type of up to 64 bits other than `bool`, the same for
both ends; `begin` not below `end` is an empty range, and the result is `init`. What `map` returns converts to `R`,
and `combine` takes two `R` and returns what converts to `R`.

## Parameters

| Parameter | Description |
|---|---|
| `begin` | the first index |
| `end` | the bound, never an index itself |
| `init` | the value on the left of the fold, and the result of an empty range |
| `map` | the value of an index, called from several threads at once through a `const` reference |
| `combine` | two values combined into one, associative; called from several threads at once through a `const` reference |
| `options` | the number of lanes and of indices a lane claims at a time ([parallel_options](parallel_options.md)) |

## Return value

The `R` of the fold.

## Complexity

`map` is called once per index and `combine` once per index and once per chunk. A loop of more than one chunk and
lane keeps a partial result per chunk until the end (a [dynamic_array](../core/dynamic_array.md) on the managed
heap: the number of chunks, the range's length divided by the grain), besides what [parallel_for](parallel_for.md)
makes; a loop of one chunk, or with one lane, folds on the caller alone.

## Exceptions

- What `map` and `combine` throw, and the copy and the move of `R`. The first exception thrown by any lane is
  rethrown from the call once every call under way has returned; the chunks nobody has claimed yet are not started,
  and the other exceptions are dropped.
- `length_error`, before `map` is called, when a loop on more than one lane has more chunks than a
  [dynamic_array](../core/dynamic_array.md) of their partial results holds: a grain of a few indices over a range of
  more than 2^59 of them.

No result is given then.

## Notes

The caller computes, and a `parallel_reduce` inside the function of another loop does not deadlock, as on the page
of [parallel_for](parallel_for.md#notes).

## Example

The sum of the squares of 1 to 100, with the default `+`:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    int64_t sum = async::parallel_reduce(1, 101, int64_t(0), [](int i) { return int64_t(i) * i; });
    println("{}", sum);
}
```

Output:

```text
338350
```

A combine that does not commute: the digits come out in the order of the indices, whatever lane took which.

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string digits = async::parallel_reduce(0, 10, string(">"), [](int i) { return to_string(i); },
                                           [](const string& a, const string& b) { return a + b; },
                                           {.grain = 1});
    println("{}", digits);
}
```

Output:

```text
>0123456789
```

In a task, the largest of the values a function gives:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

#include <algorithm>

using namespace sgcl;

async::task<int> largest() {
    co_return async::parallel_reduce(0, 1000, 0, [](int i) { return (i * 37) % 1000; },
                                     [](int a, int b) { return std::max(a, b); });
}

int main() {
    println("{}", async::spawn(largest()).wait());
}
```

Output:

```text
999
```

## See also

- [parallel_for](parallel_for.md): a function for every index
- [parallel_for_each](parallel_for_each.md): a function for every element of a range
- [parallel_options](parallel_options.md): the lanes and the grain
