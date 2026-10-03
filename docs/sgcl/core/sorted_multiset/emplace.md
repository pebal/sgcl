[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](../sorted_multiset.md)

# sgcl::sorted_multiset\<Key, Compare\>::emplace

```cpp
template<class... A>
iterator emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Constructs an element from `a...` in a new node and links the node into the tree, after the elements with an
equivalent key. The element is built before its place is known, as in `std::multiset::emplace`: the key is the
element.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments of the constructor of `Key` |

## Return value

An iterator to the inserted element.

## Complexity

Logarithmic in the size of the multiset.

## Exceptions

What the constructor of `Key` throws with `a...`; none when it is noexcept. If it throws, the multiset is as it
was.

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
    sorted_multiset<Version> versions;
    versions.emplace(2, 1);
    versions.emplace(1, 9);
    auto it = versions.emplace(2, 1);  // a second 2.1
    println("{}.{} {}", it->major, it->minor, versions.size());

    for (const auto& v : versions) {
        println("{}.{}", v.major, v.minor);
    }
}
```

Output:

```text
2.1 3
1.9
2.1
2.1
```

## See also

- [emplace_hint](emplace_hint.md): the same with a hint
- [insert](insert.md): inserts an element built already
- [sgcl::sorted_multiset\<Key, Compare\>](../sorted_multiset.md)
