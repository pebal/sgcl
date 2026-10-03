[sgcl](../../README.md) › [core](../README.md) › [sorted_set](README.md)

# sgcl::sorted_set\<Key, Compare\>::emplace_hint

```cpp
template<class... A>
iterator emplace_hint(const_iterator hint, A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Constructs an element from `a...` in a new node, as [emplace](emplace.md) does, and links it with `hint` as the
place to try first. The hint is used when the key belongs right before `hint`, or right after the largest element
when `hint` is `end()`: building a set from sorted keys at `end()` costs one comparison per key. When the key is
taken, the new element is destroyed again.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator to the element before which the new one is expected, or `end()` |
| `a` | the arguments of the constructor of `Key` |

## Return value

An iterator to the element with the key, inserted or already there.

## Complexity

Amortized constant when the element goes right before `hint`, logarithmic in the size of the set otherwise.

## Exceptions

What the constructor of `Key` throws with `a...`; none when it is noexcept. If it throws, the set is as it was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_set<string> rows;
    for (char c : string("abcde")) {
        rows.emplace_hint(rows.end(), 3, c);  // string(3, c), appended in order
    }
    println("{}", rows);

    auto it = rows.emplace_hint(rows.begin(), "ccc");  // taken: the element there
    println("{} {}", *it, rows.size());
}
```

Output:

```text
{"aaa", "bbb", "ccc", "ddd", "eee"}
ccc 5
```

## See also

- [emplace](emplace.md): the same without a hint
- [insert](insert.md): inserts an element built already, with or without a hint
- [sgcl::sorted_set\<Key, Compare\>](README.md)
