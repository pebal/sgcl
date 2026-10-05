[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [reply](README.md)

# sgcl::net::smtp::reply::to_string

```cpp
string to_string() const;
```

The reply as a line: the code, the enhanced code, the text, its lines joined by `"; "`.

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the size of the text.

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
    println("{}", net::smtp::reply{250, "2.0.0", "Ok"}.to_string());
    println("{}", net::smtp::reply{221}.to_string());
}
```

Output:

```text
250 2.0.0 Ok
221
```

## See also

- [reply](README.md)
