[sgcl](../../README.md) › [encoding](../README.md) › [uuid](README.md)

# sgcl::encoding::uuid::timestamp

```cpp
optional<time::datetime> timestamp() const noexcept;
```

The instant a UUID of the time was made, in UTC: a v1 and a v6 count 100 ns since 1582-10-15, a v7 milliseconds
since 1970. `nullopt` for every other version and for another variant. An instant before 1677, which a v1 or a v6
can hold, is the first a [datetime](../../time/datetime/README.md) holds.

## Parameters

None.

## Return value

The instant, or `nullopt`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    for (encoding::uuid id : {encoding::uuid("c232ab00-9414-11ec-b3c8-9f6bdeced846"),
                              encoding::uuid("1ec9414c-232a-6b00-b3c8-9f6bdeced846"),
                              encoding::uuid("017f22e2-79b0-7cc3-98c4-dc0c0c07398f")}) {
        println("v{} {}", id.version(), id.timestamp()->to_string());
    }
    println(encoding::uuid::v4().timestamp().has_value());
}
```

Output:

```text
v1 2022-02-22T19:22:22Z
v6 2022-02-22T19:22:22Z
v7 2022-02-22T19:22:22Z
false
```

## See also

- [v7](v7.md)
- [sgcl::encoding::uuid](README.md)
