[sgcl](../../README.md) › [core](../README.md) › [ordered_map](README.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::at

```cpp
mapped_type& at(const key_type& key);                           // (1)
const mapped_type& at(const key_type& key) const;               // (2)
template<class K> mapped_type& at(const K& key);                // (3)
template<class K> const mapped_type& at(const K& key) const;    // (4)
```

Returns a reference to the value under `key`, with a check: a key that is absent throws.

- (1–2) The key is of the key type.
- (3–4) The key is of any type the hash and the equality take. Take part only when `Hash` and `KeyEqual` both
  declare `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element |

## Return value

A reference to the value under the key.

## Complexity

Constant on average, linear in `size()` in the worst case.

## Exceptions

- `out_of_range` when the key is absent.
- (3–4) What the calls of `Hash` and `KeyEqual` with a `K` throw: none when they are noexcept or they are the
  function objects of `std`.

The map is left as it was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, int> counts = {{"x", 1}};
    counts.at("x") += 1;
    println("{}", counts.at("x"));

    try {
        counts.at("y");
    } catch (const out_of_range& e) {
        println("out of range: {}", e.what());
    }
    println("{}", counts.value_or("y", 0));  // no exception: a fallback
}
```

Output:

```text
2
out of range: sgcl::ordered_map::at
0
```

## See also

- [operator[]](operator_at.md): the value under a key, inserted when absent
- [find](find.md): an iterator to the element under a key
- [value_or](../mixin/lookup/README.md): a copy of the value under a key, or a fallback
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](README.md)
