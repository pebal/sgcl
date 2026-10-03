[sgcl](../../README.md) › [io](../README.md) › [shared_memory](../shared_memory.md)

# sgcl::io::operator==, operator!= (sgcl::io::shared_memory)

```cpp
friend bool operator==(const shared_memory& a, const shared_memory& b) noexcept;
```

Checks whether two handles are the same region, which the copies of one handle share; not the same name: two opens
of one name are two regions, and differ. Two empty handles are equal. `!=` is the negation, which C++20 writes from
`==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when both hold the same region, or neither holds one.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::shared_memory::remove("sgcl-example-cmp");
    io::shared_memory made = io::shared_memory::create("sgcl-example-cmp", 64);
    io::shared_memory copy = made;
    io::shared_memory opened = io::shared_memory::open("sgcl-example-cmp");
    println("{} {}", copy == made, opened != made);
    (void)io::shared_memory::remove("sgcl-example-cmp");
}
```

Output:

```text
true true
```

## See also

- [operator bool](operator_bool.md): whether a handle holds a region
- [sgcl::io::shared_memory](../shared_memory.md)
