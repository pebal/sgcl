[sgcl](../../README.md) › [concurrent](../README.md) › [weak_map](README.md)

# sgcl::concurrent::weak_map\<Key, T\>::find

```cpp
iterator find(const key_pointer& object) noexcept;
```

Finds the entry of `object`: a search of the table by the object's address, from the dummy node of its bucket along
the list to the entry whose weak pointer holds that address. A null pointer has no entry, and an object that is gone
has none: its entry equals nothing, so it is never found, and the entry of an object that later takes the same
address is a different one.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object whose entry to find |

## Return value

An iterator to the entry of `object`, holding its node and the object, or [end](end.md) when `object` has no entry
or is null.

## Complexity

Constant on average: the walk of the object's bucket, an entry or two.

## Exceptions

None.

## Notes

Wait-free, and writes nothing but the iterator, once the object's bucket has its dummy node; the first lookup or
insertion in a bucket that has none makes it, an allocation and a compare-exchange, lock-free, once per bucket for
the life of the array of buckets.

The iterator holds the node, so the value it reaches stays alive while the iterator exists, even when another thread
erases the entry meanwhile.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

int main() {
    concurrent::weak_map<Node, string> names;
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    names.try_emplace(a, "first");

    auto it = names.find(a);
    if (it != names.end()) {
        println("{} {}", it->key->id, it->value);
    }
    println("{}", names.find(b) == names.end());
    println("{}", names.find(nullptr) == names.end());
}
```

Output:

```text
1 first
true
true
```

## See also

- [contains](contains.md): checks whether an object has an entry
- [count](count.md): the number of entries of an object
- [sgcl::concurrent::weak_map\<Key, T\>](README.md)
