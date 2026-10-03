[sgcl](../../README.md) › [io](../README.md) › [path](README.md)

# sgcl::io::path::match

```cpp
expected<bool, error> match(const string& pattern_text, const string& name_text) noexcept;
```

Checks whether the whole name matches a shell pattern, Go's `filepath.Match`. The pattern and the name are taken
element by element, so `*` and `?` never match a separator and the two must have as many elements:

| Pattern | Matches |
|---|---|
| `*` | any run of characters, the empty one included |
| `?` | one character |
| `[a-z0-9_]` | one character of the class: characters and ranges |
| `[^a-z]`, `[!a-z]` | one character outside the class |
| `\c` | the character `c` itself, inside a class as well |

Characters are UTF-8 code points, so `?` is one character however many bytes it takes and `[а-я]` is a range of
them; an invalid byte matches only the same byte. Go takes `^` alone for the negation; `!` is the shell's.

The pattern is checked whole before anything is matched: a malformed one is an error whatever the name.

## Parameters

| Parameter | Description |
|---|---|
| `pattern_text` | the pattern |
| `name_text` | the name to match |

## Return value

Whether the name matches, or an [error](../error/README.md) with `errc::invalid_pattern`, the operation `match` and the
pattern as its path, for a malformed pattern: a `[` never closed, a `\` at the end, a range whose end is below its
start.

## Complexity

Linear in the lengths of the pattern and the name for a pattern without `*`; with one, up to their product in the
worst case, element by element.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    const char* pairs[][2] = {{"*.cpp", "main.cpp"}, {"src/*.cpp", "src/a/b.cpp"},
                              {"[а-я]*", "яблоко"}, {"?", "я"}, {"[^a-c]x", "dx"},
                              {"\\*", "*"}, {"[z-a]", "b"}};
    for (auto [pattern, name] : pairs) {
        auto matched = io::path::match(pattern, name);
        string answer = matched ? to_string(*matched) : matched.error().message();
        println("{} {}: {}", pattern, name, answer);
    }
}
```

Output:

```text
*.cpp main.cpp: true
src/*.cpp src/a/b.cpp: false
[а-я]* яблоко: true
? я: true
[^a-c]x dx: true
\* *: true
[z-a] b: match [z-a]: invalid pattern
```

## See also

- [glob](glob.md): the paths of the file system that match a pattern
- [sgcl::io::path](README.md)
