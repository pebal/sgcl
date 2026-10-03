[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md) › [builder](../map-builder.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder::set

```cpp
bool set(const Key& key, const T& value)                                            // (1)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, const Key&, const T&>);
bool set(const Key& key, T&& value)                                                 // (2)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, const Key&, T&&>);
bool set(Key&& key, const T& value)                                                 // (3)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, Key&&, const T&>);
bool set(Key&& key, T&& value)                                                      // (4)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, Key&&, T&&>);
```

Puts `value` under `key`: added when the key is absent, in place of the value there when it is present, as the
map's [set](../map/set.md). The change is made where the element lies, in a node the builder made; a node the
builder shares with a map is copied once, the first time a change goes through it.

- (1–4) The element is made of `key` and `value`, each copied or moved as it is passed.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element |
| `value` | the value to put under the key |

## Return value

`true` when the key was absent and the element added, `false` when a value was replaced.

## Complexity

Logarithmic in `size()`, base 32: a walk down the trie, a node copied the first time a change goes through it.

## Exceptions

What the copy or the move of `Key` and `T` into the element, and the copy of the elements of the nodes the
builder copies, throw; none when they are noexcept.

When an exception is thrown, the builder holds the elements it held before, and the maps it froze are untouched.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> ports = {{"http", 80}};
    auto b = ports.thaw();
    bool added = b.set("https", 443);
    bool replaced = !b.set("http", 8080);
    println("{} {} {}", added, replaced, b.freeze().at("http"));
    println("{}", ports.at("http"));
}
```

Output:

```text
true true 8080
80
```

## See also

- [insert](insert.md): adds an element only when its key is absent
- [erase](erase.md): takes out the element under a key
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder](../map-builder.md)
