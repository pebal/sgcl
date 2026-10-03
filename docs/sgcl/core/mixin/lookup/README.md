[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md)

# sgcl::mixin::lookup\<Derived\>

```cpp
#include "sgcl/core/mixin/lookup.h"   // or "sgcl/core.h"

namespace sgcl::mixin {
    template<class Derived>
    class lookup;
}
```

`sgcl::mixin::lookup<Derived>` gives a map the reads by its key that `std::map` makes a program write by hand —
the value as a copy in an optional, as a pointer into the map, or a default when the key is absent, one search
each and no exception — and declares the class a map: `req::lookup<R>` is "R carries `mixin::lookup`"
([the mixins](../README.md)). [sorted_map](../../sorted_map/README.md), [sorted_multimap](../../sorted_multimap/README.md),
[map](../../map/README.md), [multimap](../../multimap/README.md), [ordered_map](../../ordered_map/README.md) and
[immutable::map](../../../immutable/map/README.md) carry it.

The reads go through `Derived::find(key)`, an iterator, `end()` when absent, as every `find` of the library. A map
whose iterator costs more than a pointer to the value (immutable's carries a path of nodes) gives the mixin a
private `_value_of(key)`, the pointer, null when absent, and the reads go through that. The keys and the values
are views over the map's own range of pairs; `for_each` over the pairs is
[mixin::enumerable](../enumerable/README.md)'s.

## Rules

- A key of another type is accepted wherever the map's `find` is transparent: a `string` key is looked up by a
  literal or a view, and no string is made for the search.
- A map with several values per key (a multimap) gives the first by `get`, `try_get` and `value_or`, and all of
  them by [values_of](values_of.md), which exists where `equal_range` does.
- Thread safety is the map's: the members read it as its `find` does.

## Template parameters

| Parameter | Description |
|---|---|
| `Derived` | The map that carries the mixin and names itself as the argument (`class map : public mixin::lookup<map<Key, T, Hash, KeyEqual>>`): it gives `find(key)` and `end()`, or a private `_value_of(key)`, its `mapped_type`, and iterates over its pairs. Its types are named in the bodies of the members only, so the mixin is instantiated while `Derived` is not yet complete. |

## Member functions

| Function | Description |
|---|---|
| `(constructor)`, `(destructor)` | protected: the mixin exists only as a base |

#### Lookup

| Function | Description |
|---|---|
| [get](get.md) | a copy of the value under a key, an empty optional when absent |
| [try_get](try_get.md) | a pointer to the value under a key, null when absent |
| [value_or](value_or.md) | the value under a key, or a default |
| [contains_key](contains_key.md) | checks whether the map has a value under a key |
| [values_of](values_of.md) | every value under a key, as a range |

#### Views

| Function | Description |
|---|---|
| [keys](keys.md) | the keys as a range, in the map's order |
| [values](values.md) | the values as a range, in the map's order |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

// Any map of the library: what the function asks for is what the parameter says
int port_of(const req::lookup auto& ports, const char* name) {
    return ports.value_or(name, 0);
}

int main() {
    sorted_map<string, int> ports = {{"http", 80}, {"https", 443}};
    immutable::map<string, int> fixed = immutable::map<string, int>().insert("ssh", 22);
    println("{} {} {}", port_of(ports, "https"), port_of(fixed, "ssh"), port_of(fixed, "ftp"));
    println("{} {}", req::lookup<sorted_map<string, int>>, req::lookup<sorted_set<string>>);
}
```

Output:

```text
443 22 0
true false
```

## See also

- [req::lookup](../../req/lookup.md): a map read by its key: what a function asks for to call these members
- [the mixins and the requirements](../README.md); the maps' own `find`, `at`, `contains`, which `mixin::lookup`
  builds on
- `tests/core/mixin.cpp`
