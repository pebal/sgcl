[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::to_string

```cpp
string to_string() const;
```

Returns the tag in BCP-47's canonical form: the language lower-cased, the script in title case, the region in
capitals, joined by `-`, `-u-nu-latn` after them when it was asked for; `"und"` for the root locale.

## Parameters

None.

## Return value

The tag.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {} {}", txt::locale("SR_latn_rs").to_string(),
            txt::locale("ar-EG-u-nu-latn").to_string(), txt::locale().to_string());
}
```

Output:

```text
sr-Latn-RS ar-EG-u-nu-latn und
```

## See also

- [(constructor)](locale.md)
- [sgcl::txt::locale](README.md)
