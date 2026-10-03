[sgcl](../../README.md) › [core](../README.md) › [list](README.md)

# sgcl::list\<T\>::pop_back

```cpp
void pop_back() noexcept;
```

Destroys the last element and unlinks its node, which is left to the collector. The list must not be empty: on an
empty list the call is undefined, and debug builds assert.

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
    int id;
    ~Noisy() { println("~Noisy {}", id); }
};

int main() {
    list<Noisy> l;
    l.emplace_back(1);
    l.emplace_back(2);
    l.pop_back();  // the element dies here, the node later
    println("{} left, the last {}", l.size(), l.back().id);
}
```

Output:

```text
~Noisy 2
1 left, the last 1
~Noisy 1
```

## See also

- [push_back](push_back.md): appends an element
- [pop_front](pop_front.md): removes the first element
- [sgcl::list\<T\>](README.md)
