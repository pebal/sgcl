[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::dotenv::get

```cpp
optional<string> get(const string& key) const noexcept;                  // (1)
string get(const string& key, const string& fallback) const noexcept;    // (2)
```

The value of the key, shared, not copied.

1. The value, or `nullopt` when there is no such key.
2. The value, or `fallback` when there is none: `env.get("HOST", "localhost")`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key, exactly |
| `fallback` | what (2) gives when there is no value |

## Return value

(1) The value, or `nullopt`; (2) the value, or `fallback`.

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
    println("{} {}", env.get("HOST"), env.get("NONE"));
    println(env.get("NONE", "fallback"));
    println(env.get("GREETING", "?"));
}
```

Output:

```text
"example.com" nullopt
fallback
hello	world
```

## See also

- [get_int](get_int.md)
- [contains](contains.md)
- [sgcl::encoding::dotenv](README.md)
