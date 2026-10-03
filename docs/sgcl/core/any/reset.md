[sgcl](../../README.md) › [core](../README.md) › [any](README.md)

# sgcl::any::reset

```cpp
void reset() noexcept;
```

Destroys the value held, if any, now and on this thread, wherever it is: a pointer in the word, the value in the
buffer, the value in a node. The `any` is empty after. A node is left to the collector, which reclaims it with the
next sweep; the objects a pointer held are unreferenced from then on.

## Parameters

None.

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Noisy {
    tracked_ptr<int> data;  // a pointer among its data: in a node
    ~Noisy() {
        println("destroyed");
    }
};

int main() {
    any a(std::in_place_type<Noisy>, make_tracked<int>(1));
    println("before");
    a.reset();  // the destructor runs here, not in a sweep
    println("after, {}", a.has_value());
}
```

Output:

```text
before
destroyed
after, false
```

## See also

- [has_value](has_value.md): checks whether the `any` holds a value
- [emplace](emplace.md): a new value in place of the old one
- [sgcl::any](README.md)
