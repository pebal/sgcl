[sgcl](../../README.md) › [core](../README.md) › [weak_map](../weak_map.md)

# sgcl::weak_map\<Key, T\>::begin, cbegin

```cpp
/*(1)*/ iterator begin() noexcept;
/*(2)*/ const_iterator begin() const noexcept;
/*(3)*/ const_iterator cbegin() const noexcept;
```

Returns an iterator to the first live entry, or [end](end.md) when there is none. The walk visits the live entries
in no particular order, the table's, each once; it takes the object of each entry from its weak pointer as a
`tracked_ptr`, and passes over the entries whose objects are gone without dropping them.

- (1) An `iterator`, which gives out a `reference`: the object and the value as `T&`.
- (2–3) A `const_iterator`, which gives out a `const_reference`: the object and the value as `const T&`.

## Parameters

None.

## Return value

An iterator to the first live entry, or `end()`; `*it` is the object, held, and its value (`it->key`, `it->value`,
or `auto [key, value] = *it`).

## Complexity

Constant, plus the dead entries before the first live one, which the walk passes.

## Exceptions

None.

## Notes

The iterator holds the object it stands on as a strong pointer, so the entry cannot die under it; an iterator is a
tracked object then, and lives where the map's pointers may. Standing on an entry is a write to the iterator, not to
the map, so a `const` map is walked as a mutable one is. An `iterator` converts to a `const_iterator`, as those of
`std`'s containers do.

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
    vector<tracked_ptr<Node>> nodes;
    for (int i : range(1, 4)) {
        nodes.push_back(make_tracked<Node>(i));
        ranks[nodes.back()] = i * 10;
    }

    int sum = 0;
    for (auto [node, rank] : ranks) {  // node: tracked_ptr<Node>, rank: int&
        sum += node->id * rank;
        rank += 1;
    }
    println("{}", sum);

    const auto& view = ranks;
    for (auto it = view.cbegin(); it != view.cend(); ++it) {
        sum += it->value;  // a const int&
    }
    println("{}", sum);
}
```

Output:

```text
140
203
```

## See also

- [end, cend](end.md): the iterator past the last entry
- [find](find.md): an iterator to the entry of an object
- [sgcl::weak_map\<Key, T\>](../weak_map.md)
