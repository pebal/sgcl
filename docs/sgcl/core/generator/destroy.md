[sgcl](../../README.md) › [core](../README.md) › [generator](../generator.md)

# sgcl::generator\<T\>::destroy

```cpp
void destroy() noexcept;
```

Destroys the coroutine, if any, running the destructors of its locals and promise wherever it was suspended, and
leaves the generator empty; the frame's memory is the collector's once nothing refers to it. The destructor and the
move assignment do the same; `destroy` releases what the frame holds before the generator goes out of scope.

## Parameters

None.

## Return value

None.

## Complexity

The destructors of the coroutine's locals and promise.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Noisy {
    ~Noisy() {
        println("the local destroyed");
    }
};

generator<int> numbers() {
    Noisy local;
    for (int i : range(100)) {
        co_yield i;
    }
}

int main() {
    generator<int> g = numbers();
    g.next();
    g.destroy();  // the other 99 never happen
    println("{}", g.next());
}
```

Output:

```text
the local destroyed
false
```

## See also

- [frame_ptr::destroy](../frame_ptr/destroy.md): what it calls
- [sgcl::generator\<T\>](../generator.md)
