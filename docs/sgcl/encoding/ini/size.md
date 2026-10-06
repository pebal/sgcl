[sgcl](../../README.md) › [encoding](../README.md) › [ini](README.md)

# sgcl::encoding::ini::size

```cpp
size_t size() const noexcept;
```

The sections, the section `""` among them when there is.

## Parameters

None.

## Return value

The count.

## Complexity

Constant.

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
    println(config.size());
}
```

Output:

```text
3
```

## See also

- [empty](empty.md)
- [sgcl::encoding::ini](README.md)
