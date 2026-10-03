[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [lookup](README.md)

# sgcl::mixin::lookup\<Derived\>::keys

```cpp
auto keys() const noexcept;
```

Returns the keys of the map as a range: a view over the map's own range of pairs, `std::views::keys` of it, which
copies nothing and reads the map as it is iterated. The keys come in the map's order: sorted for `sorted_map`
and `sorted_multimap`, the order of insertion for `ordered_map`, no particular order for the hash maps.

## Parameters

None.

## Return value

A view of the keys, `const`, over the map; valid as long as the map, and following its changes.

## Complexity

Constant; a walk of the view is linear in the size of the map.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<string, int> ports = {{"https", 443}, {"http", 80}, {"ftp", 21}};
    vector<string> names;
    for (const string& name : ports.keys()) {
        names.push_back(name);
    }
    println("{}", names);

    ordered_map<string, int> seen;
    seen["b"] = 1;
    seen["a"] = 2;
    for (const string& name : seen.keys()) {
        print("{} ", name);
    }
    println("({} keys)", seen.size());
}
```

Output:

```text
["ftp", "http", "https"]
b a (2 keys)
```

## See also

- [values](values.md): the values as a range
- [sgcl::mixin::lookup\<Derived\>](README.md)
