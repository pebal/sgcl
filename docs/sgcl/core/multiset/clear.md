[sgcl](../../README.md) › [core](../README.md) › [multiset](../multiset.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::clear

```cpp
void clear() noexcept;
```

Destroys every element at once and unlinks every node. The bucket array, the hasher, the equality and
`max_load_factor` stay; the nodes are reclaimed by the collector later.

## Parameters

None.

## Return value

None.

## Complexity

Linear in `size()` and `bucket_count()`.

## Exceptions

None.

## Notes

Every iterator to an element is invalid afterwards; `end()` stays valid. A [rehash](rehash.md) gives the
buckets back when the multiset should shrink.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Tag {
    int id;

    explicit Tag(int i) : id(i) {}
    ~Tag() { println("~Tag {}", id); }
    bool operator==(const Tag&) const = default;
};

struct TagHash {
    size_t operator()(const Tag& t) const noexcept { return size_t(t.id); }
};

int main() {
    multiset<Tag, TagHash> s;
    s.emplace(1);
    s.emplace(1);
    println("clear");
    s.clear();
    println("{} {}", s.size(), s.bucket_count());
}
```

Output:

```text
clear
~Tag 1
~Tag 1
0 8
```

## See also

- [erase](erase.md): erases some of the elements
- [empty](empty.md): checks whether the multiset is empty
- [sgcl::multiset\<Key, Hash, KeyEqual\>](../multiset.md)
