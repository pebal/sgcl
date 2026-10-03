[sgcl](../../README.md) › [math](../README.md) › [parse_error](README.md)

# sgcl::math::operator== (sgcl::math::parse_error)

```cpp
friend bool operator==(const parse_error&, const parse_error&) noexcept = default;
```

Compares two errors member by member: equal when the reason, the offset and the base they were read in are the
same, so two equal errors have the same [message](message.md). `!=` is made from it by the compiler.
The function is a friend defined in the class, found by the arguments' type.

## Parameters

The two errors compared, unnamed in the declaration.

## Return value

Whether the errors are equal.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    auto a = math::big_integer::parse("12x4").error();
    auto b = math::big_integer::parse("12y4").error();
    auto c = math::big_integer::parse("12x4", 16).error();
    println("{} {}", a == b, a == c);
}
```

Output:

```text
true false
```

## See also

- [message](message.md): the sentence the members make
- [sgcl::math::parse_error](README.md)
