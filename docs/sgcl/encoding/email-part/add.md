[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::add

```cpp
part& add(const part& p);
```

A part after the others of a multipart: `p` itself, not a copy.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the part to add |

## Return value

`*this`.

## Complexity

Constant, amortized.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email::part alt("multipart/alternative");
    encoding::email::part plain("text/plain", "plain"), html("text/html", "<p>rich</p>");
    alt.add(plain).add(html);
    encoding::email m;
    m.set_body(alt);
    println("{} | {}", m.text(), m.html());
}
```

Output:

```text
plain | <p>rich</p>
```

## See also

- [parts](parts.md)
- [email::set_body](../email/set_body.md)
- [part](README.md)
