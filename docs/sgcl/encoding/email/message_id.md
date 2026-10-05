[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::message_id

```cpp
string message_id() const;
```

The Message-ID as written, with its angle brackets.

## Parameters

None.

## Return value

The id; `""` when there is none.

## Complexity

Linear in the number of fields.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    auto m = encoding::email::parse("Message-ID: <1234@local.machine.example>\r\n\r\n").value();
    println("{}", m.message_id());
}
```

Output:

```text
<1234@local.machine.example>
```

## See also

- [set_message_id](set_message_id.md)
- [email](README.md)
