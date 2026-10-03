[sgcl](../../README.md) › [async](../README.md) › [stopped](README.md)

# sgcl::async::stopped::message

```cpp
string message() const noexcept;
```

Returns the text of the error, `"stopped"`, for a log line or a reply.

## Parameters

None.

## Return value

The string `"stopped"`.

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
    async::stopped error;
    println("request failed: {}", error.message());
}
```

Output:

```text
request failed: stopped
```

## See also

- [timed_out::message](../timed_out/message.md): `"timed out"`
- [sgcl::async::stopped](README.md)
