[sgcl](../../README.md) › [immutable](../README.md) › [set](../set/README.md) › [builder](README.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder::freeze

```cpp
set freeze() noexcept;
```

Returns the set of what the builder holds now. The builder's marks on the nodes it made are cleared and the trie is
shared by the two: nothing is copied. The builder goes on; its next change copies again whatever the set now
shares, so nothing the builder does afterwards reaches the set.

## Parameters

None.

## Return value

A set holding the elements of the builder.

## Complexity

Linear in the number of nodes the builder made since it was thawed or last froze, at most; constant when it made
none.

## Exceptions

None.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::set<int>::builder b;
    b.insert(1);
    immutable::set<int> one = b.freeze();
    b.insert(2);
    immutable::set<int> two = b.freeze();
    println("{} {} {}", one.size(), two.size(), one.contains(2));
}
```

Output:

```text
1 2 false
```

## See also

- [set::thaw](../set/thaw.md): a builder over a set
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder](README.md)
