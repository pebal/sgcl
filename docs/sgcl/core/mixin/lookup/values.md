[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [lookup](../lookup.md)

# sgcl::mixin::lookup\<Derived\>::values

```cpp
/*(1)*/ auto values() noexcept;
/*(2)*/ auto values() const noexcept;
```

Returns the values of the map as a range: a view over the map's own range of pairs, `std::views::values` of it,
which copies nothing and reads the map as it is iterated, in the map's order.

1. The values of a map whose values are written in place may be changed through the view.
2. The values `const`.

## Parameters

None.

## Return value

A view of the values over the map; valid as long as the map, and following its changes.

## Complexity

Constant; a walk of the view is linear in the size of the map.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<string, int> ports = {{"http", 80}, {"https", 443}};
    for (int& port : ports.values()) {
        port += 8000;  // in place
    }
    vector<int> numbers;
    for (int port : ports.values()) {
        numbers.push_back(port);
    }
    println("{}", numbers);

    immutable::map<int, int> squares = immutable::map<int, int>().insert(2, 4).insert(3, 9);
    int sum = 0;
    for (int square : squares.values()) {
        sum += square;
    }
    println("{}", sum);
}
```

Output:

```text
[8080, 8443]
13
```

## See also

- [keys](keys.md): the keys as a range
- [values_of](values_of.md): the values under one key
- [sgcl::mixin::lookup\<Derived\>](../lookup.md)
