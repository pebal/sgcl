[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::dotenv::set

```cpp
dotenv set(const string& key, const string& value) const noexcept;
```

New entries with the key set to the value: in its place when it is there, at the end when it is not. The entries
themselves never change.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |
| `value` | the value |

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
    print(env.set("PORT", "9090").set("NEW", "1").erase("URL").erase("GREETING").to_string());
}
```

Output:

```text
HOST=example.com
PORT=9090
DEBUG=yes
NEW=1
```

## See also

- [erase](erase.md)
- [sgcl::encoding::dotenv](README.md)
