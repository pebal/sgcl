[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::headers

```cpp
vector<pair<string, string>> headers() const;
```

Every field of the head in its order: the name as written, the value as [header](header.md) gives it.

## Parameters

None.

## Return value

The fields.

## Complexity

Linear in the size of the head.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    auto m = encoding::email::parse("From: a@example.com\r\nSubject: Hi\r\n\r\nbody").value();
    for (auto& [name, value] : m.headers()) {
        println("{}: {}", name, value);
    }
}
```

Output:

```text
From: a@example.com
Subject: Hi
```

## See also

- [header](header.md)
- [email](README.md)
