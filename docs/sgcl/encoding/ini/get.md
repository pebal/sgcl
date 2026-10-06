[sgcl](../../README.md) › [encoding](../README.md) › [ini](README.md)

# sgcl::encoding::ini::get

```cpp
optional<string> get(const string& section, const string& key) const noexcept;                  // (1)
string get(const string& section, const string& key, const string& fallback) const noexcept;    // (2)
```

The value of the key in the section, shared, not copied; `""` is the section of the keys before the first header.

1. The value, or `nullopt` when there is no such section or key.
2. The value, or `fallback` when there is none: `config.get("server", "host", "localhost")`.

## Parameters

| Parameter | Description |
|---|---|
| `section` | the section's name, exactly |
| `key` | the key, exactly |
| `fallback` | what (2) gives when there is no value |

## Return value

(1) The value, or `nullopt`; (2) the value, or `fallback`.

## Complexity

Linear in the sections and in the section's entries.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::ini config = encoding::ini::parse(R"(name = app

; the server
[server]
host = example.com
port: 8080
tls = on
motd = Welcome!
    Second line.

[paths]
data = /var/lib/app
)").value();
    println("{} {} {}", config.get("", "name"), config.get("server", "host"), config.get("server", "none"));
    println(config.get("Server", "host", "exact names: no Server"));
}
```

Output:

```text
"app" "example.com" nullopt
exact names: no Server
```

## See also

- [get_int](get_int.md)
- [contains](contains.md)
- [sgcl::encoding::ini](README.md)
