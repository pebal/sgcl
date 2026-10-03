[sgcl](../../README.md) › [immutable](../README.md) › [map](README.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::update

```cpp
template<class F>
map update(const Key& key, F f) const                                                     // (1)
    noexcept(std::is_nothrow_invocable_v<F&, const T&> &&
             std::is_nothrow_constructible_v<T, std::invoke_result_t<F&, const T&>> &&
             std::is_nothrow_move_constructible_v<T> &&
             std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, const Key&, T&&>);
template<class F>
map update(const Key& key, const T& fallback, F f) const                                  // (2)
    noexcept(std::is_nothrow_invocable_v<F&, const T&> &&
             std::is_nothrow_constructible_v<T, std::invoke_result_t<F&, const T&>> &&
             std::is_nothrow_move_constructible_v<T> &&
             std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, const Key&, T&&>);
```

Returns the map with `f(value)` in place of the value under `key`: [set](set.md) of what `f` gives of the old value,
Clojure's `update`. This map is unchanged. `f` is called once, with the value as a `const T&`, and what it returns
is made into a `T` before the path is copied, so `f` may read this map. Takes part only when `f` called with a
`const T&` gives something a `T` is made of.

1. When the key is absent, the same map, `f` not called: immer's `update_if_exists`.
2. When the key is absent, `f(fallback)` is put under it: a count is `m.update(word, 0, [](int n) { return n + 1; })`,
   Clojure's `update` with `fnil`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element |
| `fallback` | the value `f` is given when the key is absent |
| `f` | what makes the new value of the old one |

## Return value

The new map: (1) of the same size, the same map when the key is absent; (2) one element more when the key was
absent.

## Complexity

A lookup and a call of `f`; then as [set](set.md), logarithmic in `size()`, base 32.

## Exceptions

What `f` throws, what the construction of `T` of its result and the move of `T` throw, and what the copy of the
elements on the path throws; none when they are noexcept.

This map is never changed, so an exception leaves it as it was; no new map is made.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> counts;
    for (string word : {"to", "be", "or", "not", "to", "be"}) {
        counts = counts.update(word, 0, [](int n) { return n + 1; });
    }
    println("{} {} {}", counts.at("to"), counts.at("not"), counts.size());

    auto doubled = counts.update("be", [](int n) { return n * 2; });
    auto same = counts.update("question", [](int n) { return n * 2; });
    println("{} {} {}", counts.at("be"), doubled.at("be"), same.size());
}
```

Output:

```text
2 1 4
2 4 4
```

## See also

- [set](set.md): the map with a value under a key
- [at](at.md): the value under a key
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](README.md)
