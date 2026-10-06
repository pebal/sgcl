[sgcl](../../README.md) › [encoding](../README.md) › [content_line](README.md)

# sgcl::encoding::content_line::params, param

```cpp
slice<const parameter> params() const noexcept;                             // (1)
optional<string> param(const string& name) const noexcept;                  // (2)
string param(const string& name, const string& fallback) const noexcept;    // (3)
```

1. The [parameters](../content_line-parameter.md) in order.
2. The first value of the parameter of the name (in any case), or `nullopt`.
3. The same, or `fallback`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the parameter's name |
| `fallback` | what (3) gives when there is none |

## Return value

The parameters, or the value.

## Complexity

(1) Constant; (2–3) linear in the parameters.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto line = encoding::content_line::parse("TEL;TYPE=work,voice;PREF=1:tel:+1-555-0100").value();
    for (const auto& [name, values] : line.params()) {
        println("{} = {} value(s)", name, values.size());
    }
    println("{} {} {}", line.param("type"), line.param("PREF", "0"), line.param("CN", "none"));
}
```

Output:

```text
TYPE = 2 value(s)
PREF = 1 value(s)
"work" 1 none
```

## See also

- [with_param](with_param.md)
- [sgcl::encoding::content_line](README.md)
