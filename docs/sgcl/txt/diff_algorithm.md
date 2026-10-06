[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::diff_algorithm

```cpp
#include "sgcl/txt/diff.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class diff_algorithm : uint8_t {
        myers,
        patience,
    };
}
```

How an edit script is found.

| Value | Description |
|---|---|
| `myers` | Myers' algorithm (1986) in linear space: a shortest script, the default |
| `patience` | patience diff (Bram Cohen): the lines unique to both texts aligned first, by the longest increasing run of them, Myers between them; not always shortest, but keeps moved blocks of code whole |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

// each line of the edits, marked as diff marks it
void show(const string& a, const string& b, const vector<txt::diff_edit>& edits) {
    for (const auto& e : edits) {
        bool inserted = e.kind == txt::diff_kind::insert;
        const string& text = inserted ? b : a;
        size_t at = inserted ? e.new_begin : e.old_begin, end = inserted ? e.new_end : e.old_end;
        char mark = inserted ? '+' : e.kind == txt::diff_kind::remove ? '-' : ' ';
        while (at < end) {
            size_t next = text.find('\n', at) + 1;
            print("{} {}", mark, text.substr(at, next - at));
            at = next;
        }
    }
}

int main() {
    string a = "f()\nx\nx\n}\n", b = "f()\n}\nx\n";
    show(a, b, txt::diff_lines(a, b));
    println("--");
    show(a, b, txt::diff_lines(a, b, {.algorithm = txt::diff_algorithm::patience}));
}
```

Output:

```text
  f()
- x
+ }
  x
- }
--
  f()
- x
- x
  }
+ x
```

## See also

- [diff_options](diff_options.md)
- [unified_options](unified_options.md)
- [sgcl::txt](README.md)
