[sgcl](../../README.md) › [core](../README.md) › [multimap](README.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::equal_range

```cpp
std::pair<iterator, iterator> equal_range(const key_type& key) noexcept;                      // (1)
std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const noexcept;    // (2)
template<class K>
std::pair<iterator, iterator> equal_range(const K& key) noexcept(/* see below */);            // (3)
template<class K>
std::pair<const_iterator, const_iterator> equal_range(const K& key) const                     // (4)
    noexcept(/* see below */);
```

Returns the run of the elements under `key`: they are adjacent in the iteration, the one inserted last first.

- (1–2) The key is of the key type.
- (3–4) The key is of any type the hash and the equality take. Take part only when `Hash` and `KeyEqual` both
  declare `is_transparent`, as `std::hash` and `std::equal_to` of a [string](../string/README.md) do.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the elements |

## Return value

A pair of iterators: the first element under the key and the one after the last, or two `end()` iterators when
the key is not there.

## Complexity

Constant on average, plus the number of elements under the key.

## Exceptions

- (1–2) None.
- (3–4) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are the function objects of
  `std`; otherwise what they throw.

## Notes

`values_of` of [mixin::lookup](../mixin/lookup/README.md) gives the values of the run as a range of their own.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multimap<string, int> m = {{"a", 1}, {"b", 9}};
    m.emplace("a", 2);
    m.emplace("a", 3);

    string text = "a b";
    auto [from, to] = m.equal_range(text.as_slice(0, 1));  // nothing built for the key
    vector<int> run;
    for (auto it = from; it != to; ++it) {
        run.push_back(it->second);
    }
    println("{}", run);

    int sum = 0;
    for (int v : m.values_of("a")) {
        sum += v;
    }
    println("{}", sum);
}
```

Output:

```text
[3, 2, 1]
6
```

## See also

- [find](find.md): an iterator to the first element under a key
- [count](count.md): the number of elements under a key
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](README.md)
