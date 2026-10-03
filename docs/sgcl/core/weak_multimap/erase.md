[sgcl](../../README.md) › [core](../README.md) › [weak_multimap](../weak_multimap.md)

# sgcl::weak_multimap\<Key, T\>::erase

```cpp
size_type erase(const key_pointer& object) noexcept;    // (1)
iterator erase(iterator pos) noexcept;                  // (2)
```

Erases entries and destroys their values at once.

1. Erases every entry of `object`. A null pointer has none.
2. Erases the entry `pos` stands on, and returns the next live entry up to the iterator's own bound: `end()`, or,
   for an iterator of an [equal_range](equal_range.md), the end of that range, once the object's entries are
   passed. So a walk that erases a range stops where the range does, and the entries of the other objects behind
   it are not touched. `pos` must stand on an entry, not be [end](end.md).

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object whose entries to erase |
| `pos` | an iterator to the entry to erase |

## Return value

- (1) The number of entries erased.
- (2) An iterator to the next live entry after `pos` up to its bound, or the bound.

## Complexity

- (1) Constant on average, plus linear in the number of the object's entries.
- (2) Constant on average, plus the dead entries before the next live one, which the iterator passes.

## Exceptions

None.

## Notes

An entry whose object is gone needs no erasure: it is never found, and a [sweep](sweep.md) drops it.

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
    tags.insert(b, "z");
    tags.insert(b, "w");

    size_t erased = tags.erase(a);
    println("{} {}", erased, tags.size());

    for (auto [first, last] = tags.equal_range(b); first != last;) {
        if (first->value == "z") {
            first = tags.erase(first);
        } else {
            ++first;
        }
    }
    println("{} {}", tags.count(b), tags.find(b)->value);
}
```

Output:

```text
2 2
1 w
```

## See also

- [sweep](sweep.md): erases the entries whose objects are gone
- [clear](clear.md): erases every entry
- [sgcl::weak_multimap\<Key, T\>](../weak_multimap.md)
