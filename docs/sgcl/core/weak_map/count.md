[sgcl](../../README.md) › [core](../README.md) › [weak_map](README.md)

# sgcl::weak_map\<Key, T\>::count

```cpp
size_type count(const key_pointer& object) const noexcept;
```

Returns the number of entries of `object`: 1 when it has one, 0 when it has none or is null. The search is that of
[find](find.md); an object has at most one entry.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object whose entries to count |

## Return value

The number of entries of `object`, 0 or 1.

## Complexity

Constant on average.

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
    weak_map<Node, int> ranks;
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    ranks.emplace(a, 10);
    ranks.emplace(a, 20);  // a has an entry: nothing added

    println("{} {}", ranks.count(a), ranks.count(b));
}
```

Output:

```text
1 0
```

## See also

- [contains](contains.md): the same question as a `bool`
- [find](find.md): the entry of an object
- [sgcl::weak_map\<Key, T\>](README.md)
