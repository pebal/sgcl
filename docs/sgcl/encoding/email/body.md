[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::body

```cpp
part body() const;
```

The body as a [part](../email-part/README.md): the root of the MIME tree, whose head is the message's own (its
fields are the message's fields).

## Parameters

None.

## Return value

The root part.

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
    encoding::email m("a@example.com", "b@example.com", "s", "plain");
    m.set_html("<p>x</p>");
    auto root = m.body();
    println("{}", root.content_type());
    for (auto& p : root.parts()) {
        println("  {}", p.content_type());
    }
}
```

Output:

```text
multipart/alternative
  text/plain
  text/html
```

## See also

- [set_body](set_body.md)
- [part](../email-part/README.md)
- [email](README.md)
