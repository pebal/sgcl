[sgcl](../../README.md) › [immutable](../README.md) › [set](README.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::thaw

```cpp
builder thaw() const noexcept;
```

Returns a [builder](../set-builder/README.md) over this set: a set changed in place and frozen into a set when it is
done, as the [map's](../map/thaw.md) builder is for a map. The builder starts from this set's trie, which the two
share, and copies nothing until it changes. This set is unchanged, whatever the builder does.

## Parameters

None.

## Return value

A builder holding the elements of this set.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::set<string> seen = {"alice", "bob"};
    auto b = seen.thaw();
    for (const char* name : {"dave", "erin", "alice"}) {
        b.insert(name);
    }
    auto everyone = b.freeze();
    println("{} {}", seen.size(), everyone.size());
}
```

Output:

```text
2 4
```

## See also

- [set::builder](../set-builder/README.md): the builder and its members
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](README.md)
