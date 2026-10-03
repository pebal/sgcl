[sgcl](../../README.md) › [core](../README.md) › [forward_list](README.md)

# sgcl::forward_list\<T\>::clear

```cpp
void clear() noexcept;
```

Destroys every element, from the first to the last, and unlinks every node. The unlinked nodes are left to the
collector, which reclaims them once nothing refers to them; the sentinel is the list's own and stays.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of elements.

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
    l.emplace_front(2);
    l.emplace_front(1);

    l.clear();  // the elements die here
    println("{}", l.empty());
}
```

Output:

```text
~Noisy 1
~Noisy 2
true
```

## See also

- [erase_after](erase_after.md): erases elements after a position
- [empty](empty.md): checks whether the list is empty
- [sgcl::forward_list\<T\>](README.md)
