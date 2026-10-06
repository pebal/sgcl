[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::dotenv::contains

```cpp
bool contains(const string& key) const noexcept;
```

Whether there is the key: what [get](get.md) with a fallback cannot tell from a key whose value is the fallback.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key, exactly |

## Return value

`true` when there is.

## Complexity

Linear in the entries.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::dotenv::options o;
    o.use_environment = false;
    encoding::dotenv env = encoding::dotenv::parse(R"(# the service
export HOST=example.com
PORT=8080
DEBUG=yes
GREETING="hello\tworld"
URL=http://${HOST}:${PORT}/
)", o).value();
    println("{} {} {}", env.contains("HOST"), env.contains("host"), env.contains("NONE"));
}
```

Output:

```text
true false false
```

## See also

- [get](get.md)
- [sgcl::encoding::dotenv](README.md)
