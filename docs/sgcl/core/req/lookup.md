[sgcl](../../README.md) › [core](../README.md) › [req](../req.md)

# sgcl::req::lookup

```cpp
#include "sgcl/core/req.h"   // or "sgcl/core.h"

namespace sgcl::req {
    template<class R>
    concept lookup;  // enumerable<R>, and R carries mixin::lookup
}
```

A map read by its key: [enumerable](enumerable.md), and `R` carries [mixin::lookup](../mixin/lookup.md), which
gives it `get`, `try_get`, `value_or`, `contains_key`, `keys` and `values`.

## Satisfied by

- `map`, `multimap`, `sorted_map`, `sorted_multimap`, `ordered_map`, `immutable::map`.

Not by the sets, which have keys and no values.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int price(const req::lookup auto& prices, const string& item) {
    return prices.value_or(item, 0);
}

int main() {
    map<string, int> shop = {{"apple", 3}, {"pear", 4}};
    sorted_map<string, int> market = {{"apple", 2}};
    println("{} {} {}", price(shop, "pear"), price(market, "apple"), price(market, "plum"));
    println("{}", req::lookup<set<string>>);
}
```

Output:

```text
4 2 0
false
```

## See also

- [enumerable](enumerable.md)
- [mixin::lookup](../mixin/lookup.md): the members it gives
- [sgcl::req](../req.md)
