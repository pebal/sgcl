[sgcl](../../README.md) › [core](../README.md) › [weak_map](../weak_map.md)

# sgcl::weak_map\<Key, T\>::find

```cpp
iterator find(const key_pointer& object) noexcept;                // (1)
const_iterator find(const key_pointer& object) const noexcept;    // (2)
```

Finds the entry of `object`: a search of the table by the object's address. A null pointer has no entry. A dead
entry equals nothing, so it is never found, and the entry of an object that later takes the same address is a
different one.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object whose entry to find |

## Return value

An iterator to the entry of `object`, holding the object, or [end](end.md) when `object` has no entry or is null.
(2) gives the value as `const T&`.

## Complexity

Constant on average.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

int main() {
    weak_map<Node, string> names;
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    names[a] = "first";

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
- [operator[]](operator_at.md): the value of an object, made when it has none
- [sgcl::weak_map\<Key, T\>](../weak_map.md)
