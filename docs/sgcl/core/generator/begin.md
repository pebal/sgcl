[sgcl](../../README.md) › [core](../README.md) › [generator](../generator.md)

# sgcl::generator\<T\>::begin

```cpp
iterator begin();
```

Calls [next](next.md), so it runs the coroutine to its first value, and returns an [iterator](../generator-iterator.md)
to it, or the end iterator when there is none. It is not repeatable: a second `begin()` advances the coroutine
again, as a second `next()` would.

## Parameters

None.

## Return value

An iterator to the current value, or the end iterator.

## Complexity

One resumption of the coroutine.

## Exceptions

What the coroutine throws before its first value.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

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

    generator<int> g = squares(3);
    auto first = g.begin();
    auto second = g.begin();  // one step further
    println("{} {}", *second, first == second);
}
```

Output:

```text
[1, 4, 9, 16]
4 true
```

## See also

- [end](end.md): the end iterator
- [iterator](../generator-iterator.md): what `begin` returns
- [sgcl::generator\<T\>](../generator.md)
