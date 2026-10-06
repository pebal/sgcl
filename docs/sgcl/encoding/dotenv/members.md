[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::dotenv::members

```cpp
slice<const member> members() const noexcept;
```

The [entries](../dotenv-member.md) in the file's order, a slice of the entries, nothing copied; what an
[io::command](../../io/command/README.md)'s environment is made of.

## Parameters

None.

## Return value

The entries.

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
    encoding::dotenv::options o;
    o.use_environment = false;
    encoding::dotenv env = encoding::dotenv::parse(R"(# the service
export HOST=example.com
PORT=8080
DEBUG=yes
GREETING="hello\tworld"
URL=http://${HOST}:${PORT}/
)", o).value();
    for (const auto& [key, value] : env.members()) {
        println("{} = {}", key, value);
    }
}
```

Output:

```text
HOST = example.com
PORT = 8080
DEBUG = yes
GREETING = hello	world
URL = http://example.com:8080/
```

## See also

- [get](get.md)
- [sgcl::encoding::dotenv](README.md)
