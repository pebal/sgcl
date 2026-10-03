[sgcl](../../README.md) › [txt](../README.md) › [object](README.md)

# sgcl::txt::object::set

```cpp
void set(const string& name, const value& v) noexcept;
```

Sets the value of `name` to `v`: a field at a time, for a mapping whose names are not known where the program is
written. A new name goes at the end; a name already there keeps its place and takes the new value. The mapping is
shared by every copy of the object, so every copy sees it.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name |
| `v` | its value |

## Return value

None.

## Complexity

Constant on average.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::object counts;
    for (auto word : {"to", "be", "or", "not", "to", "be"}) {
        const txt::value* seen = counts.find(word);
        counts.set(word, seen ? txt::value(seen->to_string() + "+") : txt::value("+"));
    }
    println("{}", txt::stencil("{{ range . }}{{ .key }}{{ .value }} {{ end }}").render(counts));
    return 0;
}
```

Output:

```text
to++ be++ or+ not+ 
```

## See also

- [object](object.md): the constructors
- [sgcl::txt::object](README.md)
