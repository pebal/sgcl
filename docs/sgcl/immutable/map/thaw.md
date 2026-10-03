[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::thaw

```cpp
builder thaw() const noexcept;
```

Returns a [builder](../map-builder.md) over this map: a map changed in place, one element at a time, and frozen
into a map when it is done, the transient of Clojure and immer. The builder starts from this map's trie, which the
two share, and copies nothing until it changes: the first change through a node the map holds copies that node
once, and the changes after it are made in place. This map is unchanged, whatever the builder does.

## Parameters

None.

## Return value

A builder holding the elements of this map.

## Complexity

Constant.

## Exceptions

None.

## Notes

Nothing needs a builder: a map built at once from a range is the [constructor](map.md)'s. The builder is for a map
made or changed one element at a time, with conditions between, or for many changes to a large map at once: an
edit of a tenth of 200,000 keys set and a tenth erased costs 137 ns per change through one builder, against 559 ns
made a version each ([Benchmarks: The builder](../benchmarks.md#the-builder)).

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> stock = {{"apples", 3}, {"pears", 0}};
    auto b = stock.thaw();
    for (const auto& [item, count] : stock) {
        if (count == 0) {
            b.erase(item);
        }
    }
    b.set("plums", 12);
    auto restocked = b.freeze();
    println("{} {}", stock.size(), stock.contains("pears"));
    println("{} {}", restocked.size(), restocked.contains("pears"));
}
```

Output:

```text
2 true
2 false
```

## See also

- [map::builder](../map-builder.md): the builder and its members
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](../map.md)
