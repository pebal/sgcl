[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [reply](README.md)

# sgcl::net::smtp::operator== (sgcl::net::smtp::reply)

```cpp
friend bool operator==(const reply&, const reply&) noexcept = default;
```

Whether the code, the enhanced code and the text are the same.

## Parameters

| Parameter | Description |
|---|---|
| `a, b` | the replies |

## Return value

`true` or `false`.

## Complexity

Linear in the size of the texts.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println("{}", net::smtp::reply{250, "2.0.0", "Ok"} == net::smtp::reply{250, "2.0.0", "Ok"});
}
```

Output:

```text
true
```

## See also

- [reply](README.md)
