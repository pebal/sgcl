[sgcl](../../README.md) › [txt](../README.md) › [searcher](README.md)

# sgcl::txt::searcher::searcher

```cpp
explicit searcher(const string& pattern) noexcept;
```

Prepares a pattern: keeps it and builds the table of skips from it, how far the search may move on when the byte
under the end of the pattern is any given byte. The pattern is bytes; it need not be valid UTF-8, and an empty
pattern is found at once wherever it is looked for.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the bytes to look for |

## Complexity

Linear in the length of the pattern.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::searcher crlf("\r\n");
    for (string line : {"GET / HTTP/1.1\r\n", "Host: example.com", "\r\n"}) {
        size_t at = crlf.find(line);
        print("{} ", at == npos ? "-" : txt::format("{}", at));
    }
    println();
}
```

Output:

```text
14 - 0 
```

## See also

- [find](find.md): the first occurrence
- [sgcl::txt::searcher](README.md)
