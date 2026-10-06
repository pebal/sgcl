[sgcl](../../README.md) › [core](../README.md) › [generator](README.md)

# sgcl::generator\<T\>::begin

```cpp
iterator begin() noexcept;
```

Returns an [iterator](../generator-iterator.md) at the generator's next value, without running the coroutine: the
iterator runs it, as [next](next.md) does, at its first look at a value (`*it`, `it->` or the comparison with
[end](end.md)), and its `++` only marks the value used. A range-for left by a `break`, or a `std::views::take(n)`
over the generator, has run the coroutine to the last value it looked at and no further, so the next `begin()`
goes on from the value after it.

## Parameters

None.

## Return value

An iterator at the next value, which it has not run the coroutine to yet.

## Complexity

Constant. A look at a value costs one resumption of the coroutine.

## Exceptions

None. What the coroutine throws comes out of the iterator's look that runs it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <ranges>

using namespace sgcl;

generator<int> squares(int n) {
    for (int i : range(1, n + 1)) {
        co_yield i * i;
    }
}

int main() {
    vector<int> all;
    for (int v : squares(4)) {  // the temporary generator lives for the loop
        all.push_back(v);
    }
    println("{}", all);

    generator<int> g = squares(5);
    for (int v : g | std::views::take(2)) {
        println("taken {}", v);
    }
    for (int v : g) {  // goes on from the third value
        println("then {}", v);
    }
}
```

Output:

```text
[1, 4, 9, 16]
taken 1
taken 4
then 9
then 16
then 25
```

## See also

- [end](end.md): the end of a range-for
- [iterator](../generator-iterator.md): what `begin` returns
- [sgcl::generator\<T\>](README.md)
