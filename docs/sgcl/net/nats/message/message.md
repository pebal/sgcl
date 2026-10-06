[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [message](README.md)

# sgcl::net::nats::message::message

```cpp
message();                                             // (1)
message(const string& subject, const string& data);    // (2)
```

1. An empty message.
2. A message of the subject and the data, without a reply subject or headers.

## Parameters

| Parameter | Description |
|---|---|
| `subject` | the subject |
| `data` | the bytes |


## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    net::nats::message empty;
    net::nats::message m("orders.new", "{}");
    println("[{}] {} {}", empty.subject, m.subject, m.data);
}
```

Output:

```text
[] orders.new {}
```

## See also

- [publish](../client/publish.md)
- [message](README.md)
