[sgcl](../../README.md) › [txt](../README.md) › [collated_matches](README.md)

# sgcl::txt::collated_matches::pattern

```cpp
const searcher_type& pattern() const noexcept;
```

Returns the pattern the range looks for, weighed by the range's collator: the
[collated_searcher](../collated_searcher/README.md) it was given, or the one it weighed again where that belonged to
another collator. Its
[pattern](../collated_searcher/pattern.md) is the text as it was given.

## Parameters

None.

## Return value

The searcher; one of an empty pattern for a range made by the default constructor.

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
    txt::collator root;
    txt::collator danish(txt::locale("da"), txt::strength::primary);
    txt::collated_matches matches(danish, "Aalborg", txt::collated_searcher(root, "å"));
    println("{} {} {}", matches.pattern().pattern(), matches.pattern().by() == danish,
            matches.count());
}
```

Output:

```text
å true 1
```

## See also

- [text](text.md): the text
- [sgcl::txt::collated_matches](README.md)
