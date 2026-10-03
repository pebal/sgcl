[sgcl](../../README.md) › [concurrent](../README.md) › [weak_map](README.md)

# sgcl::concurrent::weak_map\<Key, T\>::begin

```cpp
iterator begin() noexcept;
```

Returns an iterator to the first live entry, or [end](end.md) when there is none. The iterator walks the table's
list from its head, in the order of the list (the bit reversal of the hashes of the objects' addresses, an order of
no meaning to the program), and stops at the first entry whose object it can hold: it takes the object from the
entry's weak pointer as a `tracked_ptr`, and passes over the entries whose objects are gone without dropping them.
Each step of the iterator does the same, so a walk visits every live entry once.

## Parameters

None.

## Return value

An iterator to the first live entry; `*it` is a `reference`, the object, held, and its value (`it->key`,
`it->value`).

## Complexity

Constant when a live entry stands near the head of the list; at worst linear in the number of buckets in use and of
dead entries before the first live one, which the walk passes.

## Exceptions

None.

## Notes

The walk is weakly consistent: an iterator holds its node and the object it stands on, so it is valid whatever the
other threads do and the entry cannot die under it; it skips the entries erased since it passed them and may or may
not see the ones inserted meanwhile. A step never waits and writes nothing but the iterator.

`begin` is not `const` and there is no `cbegin`: standing on an entry holds its object, which is a write to the
iterator, not to the map, and the values are reached as `T&`.

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
    vector<tracked_ptr<Node>> nodes;
    for (int i : range(3)) {
        nodes.push_back(make_tracked<Node>(i));
        names.try_emplace(nodes.back(), "node " + to_string(i));
    }

    vector<string> seen;
    for (auto it = names.begin(); it != names.end(); ++it) {
        seen.push_back(to_string(it->key->id) + ": " + it->value);
    }
    seen.sort();  // the order of the table is the hashes'
    println("{}", seen);
}
```

Output:

```text
["0: node 0", "1: node 1", "2: node 2"]
```

## See also

- [end](end.md): the iterator past the last entry
- [find](find.md): an iterator to the entry of an object
- [sgcl::concurrent::weak_map\<Key, T\>](README.md)
