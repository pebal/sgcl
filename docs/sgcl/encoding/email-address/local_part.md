[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [address](README.md)

# sgcl::encoding::email::address::local_part

```cpp
string local_part() const;
```

The addr-spec before its last `@`: the mailbox's name at its domain (the whole addr-spec when it has no `@`).

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
    println("{}", encoding::email::address("john.doe@example.com").local_part());
}
```

Output:

```text
john.doe
```

## See also

- [addr](addr.md)
- [address](README.md)
