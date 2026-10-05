[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [address](README.md)

# sgcl::encoding::operator== (sgcl::encoding::email::address)

```cpp
friend bool operator==(const address& a, const address& b) noexcept;
```

Whether the two addresses have the same name and the same addr-spec, byte for byte.

## Parameters

| Parameter | Description |
|---|---|
| `a, b` | the addresses |

## Return value

`true` or `false`.

## Complexity

Linear in the size of the addresses.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email::address a("A <a@x.example>");
    encoding::email::address same("A", "a@x.example"), unnamed("", "a@x.example");
    println("{} {}", a == same, a == unnamed);
}
```

Output:

```text
true false
```

## See also

- [address](README.md)
