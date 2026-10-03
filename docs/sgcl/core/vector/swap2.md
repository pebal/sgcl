[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::swap (sgcl::vector)

```cpp
friend void swap(vector& l, vector& r) noexcept;
```

Exchanges the contents of `l` and `r`, as `l.swap(r)`. The function is a friend defined in the class, found by
the arguments' type: `swap(a, b)` and `std::ranges::swap(a, b)` call it, and so does the
`using std::swap; swap(a, b);` of generic code.

## Parameters

| Parameter | Description |
|---|---|
| `l`, `r` | the vectors to exchange the contents of |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<string> today = {"write"};
    vector<string> tomorrow = {"test", "ship"};
    swap(today, tomorrow);
    println("{} {}", today, tomorrow);
}
```

Output:

```text
["test", "ship"] ["write"]
```

## See also

- [swap](swap.md): the member form, `a.swap(b)`
- [sgcl::vector\<T\>](README.md)
