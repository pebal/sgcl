[sgcl](../../README.md) › [txt](../README.md) › [stencil_functions](../stencil_functions.md)

# sgcl::txt::stencil_functions::add

```cpp
void add(const string& name, stencil_function fn) noexcept;
```

Joins `fn` to the table under `name`, or replaces the function of that name, which is how a program overrides one
of the six. A template parsed before keeps the function it was parsed with.

The function must be pure of side effects ([stencil_function](../stencil_function.md)): a render that does not fit the
room it was given writes one step again, so the pipeline of that field runs a second time. A function that counts,
logs or reads a clock is right on most pages and wrong on some.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name a pipeline calls it by: letters, digits and `_` |
| `fn` | the function |

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
    txt::stencil_functions table;  // starts with the six
    table.add("money", [](const txt::value& v, slice<const txt::value> args) {
        return txt::value(txt::format("{:.2f} {}", v, args[0]));
    });
    auto t = txt::stencil::parse("{{ price | money \"zł\" }}", table);
    println("{}", t->render(txt::object{{"price", 12.5}}));
    return 0;
}
```

Output:

```text
12.50 zł
```

## See also

- [stencil_function](../stencil_function.md): what a function takes and answers
- [sgcl::txt::stencil_functions](../stencil_functions.md)
