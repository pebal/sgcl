[sgcl](../../README.md) › [core](../README.md) › [string](../string.md)

# sgcl::string::as_slice, operator slice_type

```cpp
slice_type as_slice() const noexcept;                            // (1)
slice_type as_slice(size_type pos, size_type n = npos) const;    // (2)
operator slice_type() const noexcept;                            // (3)
```

Returns the characters as a [slice](../slice.md), `slice<const CharT>` (a `string_slice` of a `string`), whose owner
is the string's object: a piece of the string with no copy and no lifetime to watch.

1. All the characters.
2. The characters `[pos, pos + n)`, `n` cut to the size: `as_slice(pos)` is every character from `pos` on.
3. All the characters, as (1): a string converts to a slice by itself, so a function that takes a `string_slice`
   takes a string as it is.

The slice holds the object, so it stays valid whatever happens to the string it was taken from: the variable
assigned another string, or dropped. A slice of the empty string is empty and holds nothing.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the position of the first character of the slice |
| `n` | the number of characters, at most `size() - pos` |

## Return value

A slice of the characters, which holds the string's object.

## Complexity

Constant.

## Exceptions

- (1), (3) None.
- (2) `out_of_range` when `pos > size()`.

## Notes

A string made of a slice that is the whole of a string is that string's object again, with no copy; a string made of
a part is a new string of the part's characters ([constructor](string.md), 12). The pieces of
[split](split.md) and [fields](fields.md) are such slices. A text slice shares the read interface of a string,
[mixin::text](../mixin/text.md): `find`, `starts_with`, `trim`, the comparisons.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

size_t letters(const string_slice& text) {
    size_t n = 0;
    for (char c : text) {
        n += c >= 'a' && c <= 'z' ? 1 : 0;
    }
    return n;
}

int main() {
    string line = "hello, world";
    string kept = line;
    string_slice all = line;
    string_slice hello = line.as_slice(0, 5);
    string_slice world = line.as_slice(7);
    println("{} {} {}", hello, world, letters(line));

    line = "replaced";  // the slices hold the old object
    println("{} {} {}", line, all, world);
    string again(all), part(world);
    println("{} {}", again.object() == kept.object(), part.object() == kept.object());
}
```

Output:

```text
hello world 10
replaced hello, world world
true false
```

## See also

- [slice](../slice.md): a view of elements that holds their buffer
- [substr](substr.md): a part of the characters as a new string
- [data](data.md): the characters as a plain pointer
- [sgcl::string](../string.md)
