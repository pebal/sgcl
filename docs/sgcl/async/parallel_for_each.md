[sgcl](../README.md) › [async](README.md)

# sgcl::async::parallel_for_each

```cpp
#include "sgcl/async/parallel.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class R, class F>
    void parallel_for_each(R&& range, F f, const parallel_options& options = {})
        noexcept(/* see below */);
}
```

Calls `f` for every element of a range, the calls spread over the workers of the [scheduler](scheduler/README.md) and the
caller as [parallel_for](parallel_for.md) spreads them over indices: `std::for_each(std::execution::par, ...)`. The
loop runs inside the call, which returns once `f` has been called for every element, on a thread and in a task
alike.

`f(x)` is called with a reference to the element, so a function that takes `int&` writes the element in place, or
`f(x, lane)` when it takes two arguments, the lane as [parallel_for](parallel_for.md) gives it. The range is any
random-access range that knows its size: a [vector](../core/vector/README.md), a [slice](../core/slice/README.md), an
[array](../core/array/README.md), a `std::vector`, a plain array. The range is referred to, not copied, whether it is
given as an lvalue or as an rvalue: a temporary lives until the call returns.

## Parameters

| Parameter | Description |
|---|---|
| `range` | the elements, random access, with a size |
| `f` | called with every element, or with every element and a lane, from several threads at once; called through a `const` reference |
| `options` | the number of lanes and of elements a lane claims at a time ([parallel_options](parallel_options.md)) |

## Return value

None. The call returns once `f` has been called for every element, or once the loop has stopped at an exception.

## Complexity

`f` is called once per element; the rest as for [parallel_for](parallel_for.md#complexity).

## Exceptions

What `f` throws, the first exception of any lane rethrown from the call once every call under way has returned, as
for [parallel_for](parallel_for.md#exceptions); and what the range's `size`, `begin` and the subscript of its
iterator throw. The call is noexcept when none of these throws: the call of `f`, `f(x)` or `f(x, lane)` as the loop
calls it, and the range's `size`, `begin` and subscript.

## Example

Every element multiplied in place:

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<int> v = {1, 2, 3, 4, 5};
    async::parallel_for_each(v, [](int& x) { x *= 10; });
    println("{}", v);
}
```

Output:

```text
[10, 20, 30, 40, 50]
```

The letters of words counted on scratch space of each lane, in a task:

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<size_t> letters(vector<string> words) {
    vector<size_t> counts(async::scheduler::workers());
    async::parallel_for_each(words, [&](const string& w, unsigned lane) { counts[lane] += w.size(); });
    size_t total = 0;
    for (size_t c : counts) {
        total += c;
    }
    co_return total;
}

int main() {
    println("{}", async::spawn(letters({"one", "two", "three", "four"})).wait());
}
```

Output:

```text
15
```

## See also

- [parallel_for](parallel_for.md): a function for every index
- [parallel_reduce](parallel_reduce.md): a value of every index, combined in order
- [parallel_options](parallel_options.md): the lanes and the grain
