[sgcl](../../README.md) › [txt](../README.md) › [searcher](../searcher.md)

# sgcl::txt::searcher::find

```cpp
/*(1)*/ size_t find(const string& text, size_t from = 0) const noexcept;
/*(2)*/ size_t find(const slice<const char>& text, size_t from = 0) const noexcept;
/*(3)*/ template<size_t N> size_t find(const char (&text)[N], size_t from = 0) const noexcept;
/*(4)*/ template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
        size_t find(P text, size_t from = 0) const noexcept;
```

Finds the first occurrence of the pattern in the text at or after the byte `from`, as `std::string::find` does with
the pattern given each time.

- (1–4) The text as a string, a slice, an array of `char` up to its first NUL or its end, or the characters at a
  pointer up to their NUL.

An empty pattern is found at `from` while `from` is not past the end of the text, as it is in a `std::string`.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to search |
| `from` | the byte the search starts at |

## Return value

The byte position of the first occurrence at or after `from`, or `npos` when there is none.

## Complexity

About *n/m* steps on ordinary text, *n* the bytes after `from` and *m* the length of the pattern; *n·m* at worst.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::searcher needle("ść");
    string text = "miłość i radość";
    for (size_t at = needle.find(text); at != npos; at = needle.find(text, at + 1)) {
        print("{} ", at);
    }
    println();
    println("{}", txt::searcher("").find("abc", 3));
}
```

Output:

```text
5 16 
3
```

## See also

- [contains](contains.md): whether there is an occurrence
- [count](count.md): the number of occurrences
- [sgcl::txt::searcher](../searcher.md)
