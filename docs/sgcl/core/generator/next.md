[sgcl](../../README.md) › [core](../README.md) › [generator](../generator.md)

# sgcl::generator\<T\>::next

```cpp
bool next();
```

Runs the coroutine to its next `co_yield`, where [value](value.md) is the value it yielded, or to its end. On an
empty or finished generator it returns `false` at once. The coroutine runs on the calling thread, inside this call.

## Parameters

None.

## Return value

`true` when the coroutine yielded a value, `false` when it ended.

## Complexity

One resumption of the coroutine: what its body runs until the next `co_yield`.

## Exceptions

What the coroutine throws, rethrown here; the generator is finished after it, and the next `next()` returns `false`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

generator<int> countdown(int from) {
    for (int i : range(from)) {
        co_yield from - i;
    }
    throw runtime_error("liftoff");
}

int main() {
    generator<int> g = countdown(3);
    try {
        while (g.next()) {
            println("{}", g.value());
        }
    } catch (const runtime_error& e) {
        println("{}", e.what());
    }
    println("{}", g.next());
}
```

Output:

```text
3
2
1
liftoff
false
```

## See also

- [value](value.md): the value of the last `co_yield`
- [begin](begin.md): the same first step, as an iterator
- [sgcl::generator\<T\>](../generator.md)
