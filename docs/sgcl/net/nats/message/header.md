[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [message](README.md)

# sgcl::net::nats::message::header

```cpp
string header(const string& name) const;
```

The first value of the header, its name compared without case.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the header's name |


## Return value

The value; empty for a header the message lacks.

## Complexity

Linear in the headers.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    net::nats::message m("s", "d");
    m.headers = {{"Content-Type", "text/plain"}};
    println("{} [{}]", m.header("content-type"), m.header("Missing"));
}
```

Output:

```text
text/plain []
```

## See also

- [message](README.md)
- [message](README.md)
