[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md) › [builder](../map-builder.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder::freeze

```cpp
map freeze() noexcept;
```

Returns the map of what the builder holds now. The builder's marks on the nodes it made are cleared, a walk of
those nodes alone, and the trie is shared by the two: nothing is copied. The builder goes on; its next change
copies again whatever the map now shares, so the map is a value like any other and nothing the builder does
afterwards reaches it.

## Parameters

None.

## Return value

A map holding the elements of the builder.

## Complexity

Linear in the number of nodes the builder made since it was thawed or last froze, at most; constant when it made
none.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<int, int>::builder b;
    vector<immutable::map<int, int>> versions;
    for (int i : range(3)) {
        b.set(0, i);
        b.insert(i, i * 10);
        versions.push_back(b.freeze());  // each version stays what it was
    }
    for (const auto& m : versions) {
        println("{} elements, 0 -> {}", m.size(), m.at(0));
    }
}
```

Output:

```text
1 elements, 0 -> 0
2 elements, 0 -> 1
3 elements, 0 -> 2
```

## See also

- [map::thaw](../map/thaw.md): a builder over a map
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder](../map-builder.md)
