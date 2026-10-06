[sgcl](../../README.md) › [time](../README.md) › [cron](README.md)

# sgcl::time::cron::to_string

```cpp
string to_string() const noexcept;
```

Returns the expression as it was given, white space and all: for a log line, or to be read again with
[parse](parse.md).

## Parameters

None.

## Return value

The expression.

## Complexity

Constant: the string is shared.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::cron c("@weekly", time::zone::utc());
    println("{}", c.to_string());
}
```

Output:

```text
@weekly
```

## See also

- [parse](parse.md)
- [sgcl::time::cron](README.md)
