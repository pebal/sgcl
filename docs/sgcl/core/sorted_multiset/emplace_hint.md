[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](README.md)

# sgcl::sorted_multiset\<Key, Compare\>::emplace_hint

```cpp
template<class... A>
iterator emplace_hint(const_iterator hint, A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Constructs an element from `a...` in a new node, as [emplace](emplace.md) does, and links it as close to `hint` as
the order allows. The element goes right before `hint` when its key fits there, and right after the largest
element when `hint` is `end()` and the key is not less than it: building a multiset from sorted keys at `end()`
costs one comparison per key. Otherwise the key is searched from the root, and the element goes after its
equivalent keys when `hint` is after them, before them when `hint` is before them.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator to the element before which the new one is expected, or `end()` |
| `a` | the arguments of the constructor of `Key` |

## Return value

An iterator to the inserted element.

## Complexity

Amortized constant when the element goes right before `hint`, logarithmic in the size of the multiset otherwise.

## Exceptions

What the constructor of `Key` throws with `a...`; none when it is noexcept. If it throws, the multiset is as it
was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multiset<string> rows;
    for (char c : string("abbc")) {
        rows.emplace_hint(rows.end(), 2, c);  // string(2, c), appended in order
    }
    println("{}", rows);

    auto it = rows.emplace_hint(rows.begin(), "bb");
    println("{} {}", *it, rows.size());
}
```

Output:

```text
{"aa", "bb", "bb", "cc"}
bb 5
```

## See also

- [emplace](emplace.md): the same without a hint
- [insert](insert.md): inserts an element built already, with or without a hint
- [sgcl::sorted_multiset\<Key, Compare\>](README.md)
