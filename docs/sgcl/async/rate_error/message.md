[sgcl](../../README.md) › [async](../README.md) › [rate_error](README.md)

# sgcl::async::rate_error::message

```cpp
string message() const noexcept;
```

Returns the text of the error, for a log line or a reply: `"stopped"`, `"the tokens would come after the deadline"`
or `"more tokens than the burst"`.

## Parameters

None.

## Return value

The text of the [reason](../rate_error-reason.md).

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::rate_error e(async::rate_error::reason::deadline);
    println("request refused: {}", e.message());
}
```

Output:

```text
request refused: the tokens would come after the deadline
```

## See also

- [why](why.md): the reason itself
- [sgcl::async::rate_error](README.md)
