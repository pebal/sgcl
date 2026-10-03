[sgcl](../../README.md) › [core](../README.md) › [weak_multimap](README.md)

# sgcl::weak_multimap\<Key, T\>::contains

```cpp
bool contains(const key_pointer& object) const noexcept;
```

Checks whether `object` has an entry: [count](count.md) compared with zero. A null pointer has no entry.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to look for |

## Return value

`true` when `object` has an entry, `false` otherwise.

## Complexity

Constant on average, plus linear in the number of the object's entries, which the count walks.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

int main() {
    weak_multimap<Node, string> tags;
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    tags.insert(a, "x");
    tags.insert(a, "y");

    println("{} {}", tags.contains(a), tags.contains(b));
    tags.erase(a);
    println("{}", tags.contains(a));
}
```

Output:

```text
true false
false
```

## See also

- [count](count.md): the number of entries of an object
- [find](find.md): the first entry of an object
- [sgcl::weak_multimap\<Key, T\>](README.md)
