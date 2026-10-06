[sgcl](../../README.md) › [encoding](../README.md) › [ini](README.md)

# sgcl::encoding::ini::set

```cpp
ini set(const string& section, const string& key, const string& value) const noexcept;
```

New sections with the key set to the value in the section: in its place when it is there, at the section's end
when it is not; the section made at the end when there is none (the section `""` at the start, where a file has
it). The value itself never changes.

## Parameters

| Parameter | Description |
|---|---|
| `section` | the section's name |
| `key` | the key |
| `value` | the value |

## Return value

The new sections.

## Complexity

Linear in the size of the sections.

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
    print(config.set("server", "port", "9090").set("logging", "level", "debug").erase("paths").erase("server", "motd").to_string());
}
```

Output:

```text
name = app

[server]
host = example.com
port = 9090
tls = on

[logging]
level = debug
```

## See also

- [erase](erase.md)
- [sgcl::encoding::ini](README.md)
