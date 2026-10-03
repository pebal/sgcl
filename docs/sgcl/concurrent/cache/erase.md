[sgcl](../../README.md) › [concurrent](../README.md) › [cache](../cache.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>::erase

```cpp
bool erase(const Key& key) noexcept;                    // (1)
template<class K> bool erase(const K& key) noexcept;    // (2)
```

Erases the entry under `key`, if there is one.

1. Erases the entry of `key`.
2. Erases the entry of a key of another type, with no `Key` built for the search: a `string_view` or a literal for
   a `string` key. Takes part only when `Hash` and `KeyEqual` both have `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the entry to erase |

## Return value

`true` when this call erased an entry, `false` when there was none, or another thread was erasing it at the moment.

## Complexity

Constant on average: the map's search and its erasure.

## Exceptions

None.

## Notes

Lock-free: the entry is claimed by a flag, which exactly one thread sets, and that thread unlinks the node and
counts the entry off; a `put` racing with the erasure puts its value again as a new entry, so it is never lost. The
entry is not destroyed at the erasure: a thread reading it across keeps the node alive, and the collector destroys
it once nothing holds it. A value copied out by an earlier `get` is the caller's and lives on.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::cache<string, int> ports(10);
    ports.put("http", 80);
    ports.put("https", 443);

    optional<int> kept = ports.get("http");
    bool erased = ports.erase("http");  // a literal: no string made for the search
    bool again = ports.erase("http");
    println("{} {} {}", erased, again, ports.size());
    println("{} {}", ports.get("http").has_value(), *kept);
}
```

Output:

```text
true false 1
false 80
```

## See also

- [clear](clear.md): erases every entry
- [put](put.md): inserts or replaces a value
- [sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>](../cache.md)
