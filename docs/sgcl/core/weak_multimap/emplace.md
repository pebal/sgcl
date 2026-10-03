[sgcl](../../README.md) › [core](../README.md) › [weak_multimap](../weak_multimap.md)

# sgcl::weak_multimap\<Key, T\>::emplace

```cpp
template<class... A> iterator emplace(const key_pointer& object, A&&... a);
```

Inserts one more value for `object`, constructed in place in the entry as `T(std::forward<A>(a)...)`, whatever
entries the object has already. The entry goes in front of the object's others: the newest first, as in
[multimap](../multimap.md).

`object` may not be null: a null pointer is not an object, and debug builds assert.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to attach the value to |
| `a` | the arguments the value is constructed from |

## Return value

An iterator to the inserted entry.

## Complexity

Constant on average, plus, when the insertion brings the count since the last sweep past the threshold, a
[sweep](sweep.md), linear in the number of entries: amortized constant, as the threshold is the number of entries
the map had after the last sweep, 16 at least.

## Exceptions

What the constructor of `T` throws.

When it throws, nothing is inserted and the map is as it was.

## Notes

The call is not `noexcept`, even for a value whose construction is: the entry, a pair of the weak pointer and the
value, is built by the piecewise constructor of `std::pair`, which `std` does not declare `noexcept`.

Every insertion counts towards the next sweep, which the insertion that brings the count past the threshold runs
before it returns.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

struct Position {
    double x, y;
};

int main() {
    weak_multimap<Node, Position> path;
    tracked_ptr node = make_tracked<Node>(1);

    auto it = path.emplace(node, 1.5, 2.0);
    println("({}, {})", it->value.x, it->value.y);

    path.emplace(node, 3.0, 4.5);
    for (auto [first, last] = path.equal_range(node); first != last; ++first) {
        println("({}, {})", first->value.x, first->value.y);
    }
}
```

Output:

```text
(1.5, 2)
(3, 4.5)
(1.5, 2)
```

## See also

- [insert](insert.md): inserts a copy or a moved value
- [equal_range](equal_range.md): the entries of an object
- [sgcl::weak_multimap\<Key, T\>](../weak_multimap.md)
