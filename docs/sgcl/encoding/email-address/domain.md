[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [address](README.md)

# sgcl::encoding::email::address::domain

```cpp
string domain() const;
```

The addr-spec after its last `@`, the domain an exchanger is looked up for; `""` when it has no `@`.

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the size of the addr-spec.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", encoding::email::address("john.doe@example.com").domain());
}
```

Output:

```text
example.com
```

## See also

- [addr](addr.md)
- [address](README.md)
