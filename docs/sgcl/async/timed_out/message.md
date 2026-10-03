[sgcl](../../README.md) › [async](../README.md) › [timed_out](../timed_out.md)

# sgcl::async::timed_out::message

```cpp
string message() const noexcept;
```

Returns the text of the error, `"timed out"`, for a log line or a reply.

## Parameters

None.

## Return value

The string `"timed out"`.

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
    async::timed_out error;
    println("request failed: {}", error.message());
}
```

Output:

```text
request failed: timed out
```

## See also

- [stopped::message](../stopped/message.md): `"stopped"`
- [sgcl::async::timed_out](../timed_out.md)
