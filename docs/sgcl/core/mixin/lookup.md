# sgcl::mixin::lookup

```cpp
#include "sgcl/core/mixin/lookup.h"   // or "sgcl/core/mixin/mixin.h", "sgcl/sgcl.h"

namespace sgcl::mixin {
    template<class Derived>
    class lookup;
}
```

`mixin::lookup<Derived>` gives a map the reads by its key that `std::map` makes a program write by hand — the value as a copy in an optional, as a pointer into the map, or a default when the key is absent, one search each and no exception — and declares the class a map: `req::lookup<R>` is "R carries `mixin::lookup`" ([the mixins](README.md)). Over `Derived::find(key)`, an iterator, `end()` when absent, as every `find` of the library, and `mapped_type`; a map whose iterator costs more than a pointer to the value (immutable's carries a path of nodes) gives the mixin a private `_value_of(key)`, the pointer, and the reads go through that. [sorted_map](../sorted_map.md), [sorted_multimap](../sorted_multimap.md), [map](../map.md), [multimap](../multimap.md), [ordered_map](../ordered_map.md) and [immutable::map](../../immutable/map.md) carry it. A key of another type is accepted wherever the map's `find` is transparent; `values_of` exists where `equal_range` does.

## Members

```cpp
template<class K> optional<mapped_type> get(const K& key) const;       // a copy of the value, nullopt when absent
template<class K> mapped_type* try_get(const K& key) noexcept;         // a pointer into the map, null when absent; and const
template<class K, class U> mapped_type value_or(const K& key, U&& fallback) const;
template<class K> bool contains_key(const K& key) const;
auto keys() const;                                                     // the keys as a range, in the map's order
auto values();  auto values() const;                                   // the values as a range
template<class K> auto values_of(const K& key);                        // every value under the key (a multimap's), as a range; and const
```

```cpp
sorted_map<string, int> ports = {{"http", 80}, {"https", 443}};
assert(*ports.get("http") == 80 && !ports.get("ftp"));
assert(ports.value_or("ftp", 21) == 21 && ports.contains_key("https"));
if (int* p = ports.try_get("http")) {
    *p = 8080;                                   // in place
}
int sum = 0;
for (int port : ports.values()) {
    sum += port;                                 // 8523
}
sorted_multimap<int, string> names = {{1, "a"}, {1, "b"}};
size_t n = 0;
for (const auto& name : names.values_of(1)) {
    n += name.size();                            // 2
}
immutable::map<int, int> im = immutable::map<int, int>().insert(1, 10);
assert(*im.get(1) == 10 && im.value_or(2, 0) == 0);   // the same reads on the immutable map
```

## See also

- [the mixins and the requirements](README.md); the maps' own `find`, `at`, `contains`, which `mixin::lookup` builds on
