[sgcl](../../README.md) › [core](../README.md) › [forward_list](README.md)

# sgcl::forward_list\<T\>::pop_front

```cpp
void pop_front() noexcept;
```

Destroys the first element and unlinks its node, which is left to the collector. The list must not be empty: on an
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
    forward_list<Noisy> l;
    l.emplace_front(1);
    l.emplace_front(2);
    l.pop_front();  // the element dies here, the node later
    println("the first now {}", l.front().id);
}
```

Output:

```text
~Noisy 2
the first now 1
~Noisy 1
```

## See also

- [push_front](push_front.md): inserts an element at the beginning
- [erase_after](erase_after.md): erases elements after a position
- [sgcl::forward_list\<T\>](README.md)
