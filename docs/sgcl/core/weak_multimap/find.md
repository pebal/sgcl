[sgcl](../../README.md) › [core](../README.md) › [weak_multimap](README.md)

# sgcl::weak_multimap\<Key, T\>::find

```cpp
iterator find(const key_pointer& object) noexcept;                // (1)
const_iterator find(const key_pointer& object) const noexcept;    // (2)
```

Finds the first entry of `object`, the newest: a search of the table by the object's address. A null pointer has
no entry. A dead entry equals nothing, so it is never found, and the entries of an object that later takes the same
address are different ones.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object whose entry to find |

## Return value

An iterator to the first entry of `object`, holding the object, or [end](end.md) when `object` has no entry or is
null. (2) gives the value as `const T&`.

## Complexity

Constant on average.

## Exceptions

None.

## Notes

The iterator walks on to the end of the table, past the object's run; the entries of the object alone are what
[equal_range](equal_range.md) gives.

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
    tags.insert(a, "old");
    tags.insert(a, "new");

    auto it = tags.find(a);
    if (it != tags.end()) {
        println("{} {}", it->key->id, it->value);
    }
    println("{}", tags.find(b) == tags.end());
}
```

Output:

```text
1 new
true
```

## See also

- [equal_range](equal_range.md): the entries of an object
- [contains](contains.md): checks whether an object has an entry
- [sgcl::weak_multimap\<Key, T\>](README.md)
