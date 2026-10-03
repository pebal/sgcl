[sgcl](../../README.md) › [core](../README.md) › [sorted_set](README.md)

# sgcl::sorted_set\<Key, Compare\>::emplace

```cpp
template<class... A>
pair<iterator, bool> emplace(A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Constructs an element from `a...` in a new node and links the node into the tree unless its key is already there.
The element is built before its place is known, as in `std::set::emplace`: the key is the element, and the set
compares the element it built. When the key is taken, the new element is destroyed again and the node is left to
the collector.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments of the constructor of `Key` |

## Return value

The element with the key and `true`, or the element already there and `false`.

## Complexity

Logarithmic in the size of the set.

## Exceptions

What the constructor of `Key` throws with `a...`; none when it is noexcept. If it throws, the set is as it was.

## Notes

[insert](insert.md) of a `Key` builds nothing when the key is there; `emplace` always builds one element. A key
made of several arguments, or not movable, is what `emplace` is for.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Version {
    int major;
    int minor;

    Version(int major, int minor) : major(major), minor(minor) {}
    Version(const Version&) = delete;  // emplace needs no copy and no move

    auto operator<=>(const Version&) const = default;
};

int main() {
    sorted_set<Version> versions;
    versions.emplace(2, 1);
    versions.emplace(1, 9);
    auto [it, fresh] = versions.emplace(2, 1);
    println("{} {}.{}", fresh, it->major, it->minor);

    for (const auto& v : versions) {
        println("{}.{}", v.major, v.minor);
    }
}
```

Output:

```text
false 2.1
1.9
2.1
```

## See also

- [emplace_hint](emplace_hint.md): the same with a hint
- [insert](insert.md): inserts an element built already
- [sgcl::sorted_set\<Key, Compare\>](README.md)
