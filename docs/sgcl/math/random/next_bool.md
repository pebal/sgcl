[sgcl](../../README.md) › [math](../README.md) › [random](../random.md)

# sgcl::math::random::next_bool

```cpp
bool next_bool() noexcept;
```

A coin: `true` or `false`, each as likely, the top bit of a draw.

## Parameters

None.

## Return value

The value drawn.

## Complexity

Constant: one draw.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::random r(42);
    string tosses;
    int heads = 0;
    for (int i : range(20)) {
        bool head = r.next_bool();
        heads += head;
        tosses = tosses + (head ? "H" : "T");
    }
    println("{} {}", tosses, heads);
}
```

Output:

```text
HTTHHTTHTHHHHHHTTTTH 11
```

## See also

- [next_int](next_int.md): one of more than two values
- [sgcl::math::random](../random.md)
