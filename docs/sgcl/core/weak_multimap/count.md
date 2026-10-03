[sgcl](../../README.md) › [core](../README.md) › [weak_multimap](README.md)

# sgcl::weak_multimap\<Key, T\>::count

```cpp
size_type count(const key_pointer& object) const noexcept;
```

Returns the number of entries of `object`: 0 when it has none or is null. The search is that of [find](find.md),
and the count walks the object's run.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object whose entries to count |

## Return value

The number of entries of `object`.

## Complexity

Constant on average, plus linear in the number of the object's entries.

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
    weak_multimap<Node, int> scores;
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    scores.insert(a, 10);
    scores.insert(a, 20);
    scores.insert(a, 10);

    println("{} {}", scores.count(a), scores.count(b));
}
```

Output:

```text
3 0
```

## See also

- [contains](contains.md): checks whether an object has an entry
- [equal_range](equal_range.md): the entries of an object
- [sgcl::weak_multimap\<Key, T\>](README.md)
