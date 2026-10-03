[sgcl](../../README.md) › [txt](../README.md) › [value](../value.md)

# sgcl::txt::value::to_string

```cpp
string to_string() const;
```

The value as text, as [format](../format.md) writes it with no specification: text as it is, without quotes;
nothing as the empty string; a number, a truth, a list or a mapping as `"{}"` writes them, the text inside a list or
a mapping in quotes. It is what a function of the pipeline calls when it wants characters, whatever the value is.

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the length of the text.

## Exceptions

`length_error` when the text passes 4 GiB, the most a string holds.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value values[] = {"Ada", 1815, 2.5, true, nullptr, txt::list{1, "two"}};
    for (auto& v : values) {
        print("[{}] ", v.to_string());
    }
    println("");
    return 0;
}
```

Output:

```text
[Ada] [1815] [2.5] [true] [] [[1, "two"]] 
```

## See also

- [text](text.md): the text a value holds, with no conversion
- [sgcl::txt::value](../value.md)
