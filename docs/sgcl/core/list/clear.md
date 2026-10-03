[sgcl](../../README.md) › [core](../README.md) › [list](README.md)

# sgcl::list\<T\>::clear

```cpp
void clear() noexcept;
```

Destroys every element, from the first to the last, and unlinks every node; the sentinel stays, and so does
`end()`. The unlinked nodes are left to the collector, which reclaims them once nothing refers to them.

## Parameters

None.

## Return value

None.

## Complexity

Linear in `size()`.

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
    auto end = l.end();

    l.clear();  // the elements die here
    println("{} {}", l.size(), end == l.end());
}
```

Output:

```text
~Noisy 1
~Noisy 2
0 true
```

## See also

- [erase](erase.md): erases elements at a position or in a range
- [empty](empty.md): checks whether the list is empty
- [sgcl::list\<T\>](README.md)
