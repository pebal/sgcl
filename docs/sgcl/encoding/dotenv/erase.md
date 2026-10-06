[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::dotenv::erase

```cpp
dotenv erase(const string& key) const noexcept;
```

New entries without the key; the entries as they are when there is none.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |

## Return value

The new entries.

## Complexity

Linear in the size of the entries.

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
    println("{} {}", env.erase("HOST").size(), env.erase("NONE").size());
}
```

Output:

```text
4 5
```

## See also

- [set](set.md)
- [sgcl::encoding::dotenv](README.md)
