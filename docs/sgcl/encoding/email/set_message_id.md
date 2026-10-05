[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::set_message_id

```cpp
email& set_message_id(const string& id);
```

The Message-ID, given with or without its angle brackets. From then on it does not move with From.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the id, "left@right" |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email m;
    m.set_message_id("order-42@shop.example");
    println("{}", m.message_id());
}
```

Output:

```text
<order-42@shop.example>
```

## See also

- [message_id](message_id.md)
- [email](README.md)
