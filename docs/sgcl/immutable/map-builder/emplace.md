[sgcl](../../README.md) › [immutable](../README.md) › [map](../map/README.md) › [builder](README.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder::emplace

```cpp
template<class... A>
bool emplace(const Key& key, A&&... a)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_copy_constructible_v<Key> && std::is_nothrow_constructible_v<T, A...>);
```

Adds an element under `key` whose value is constructed from `a...`, when the key is absent; does nothing when it
is there, as [insert](insert.md).

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element, copied into it |
| `a` | the arguments the value is constructed from |

## Return value

`true` when the element was added, `false` when the key was there.

## Complexity

Logarithmic in `size()`, base 32: the key looked up, then a walk down the trie, a node copied the first time a
change goes through it.

## Exceptions

What the copy of `Key`, the constructor of `T` from `a...` and the copy of the elements of the nodes the
builder copies throw; none when they are noexcept.

When an exception is thrown, the builder holds the elements it held before, and the maps it froze are untouched.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<int, string>::builder rules;
    rules.emplace(1, 5, '=');  // the value is string(5, '=')
    bool again = rules.emplace(1, 3, '-');
    println("{} {}", *rules.try_get(1), again);
}
```

Output:

```text
===== false
```

## See also

- [insert](insert.md): adds an element when its key is absent
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder](README.md)
