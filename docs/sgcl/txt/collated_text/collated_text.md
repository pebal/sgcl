[sgcl](../../README.md) › [txt](../README.md) › [collated_text](README.md)

# sgcl::txt::collated_text::collated_text

```cpp
collated_text() noexcept = default;                                                  // (1)
collated_text(const collator& by, const string& text) noexcept;                      // (2)
collated_text(const collator& by, const slice<const char>& text);                    // (3)
template<size_t N> collated_text(const collator& by, const char (&text)[N]);         // (4)
template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
collated_text(const collator& by, P text);                                           // (5)
```

Weighs a text once by a collator, keeping a copy of the collator and the text.

1. An empty text, of the root collator.
2. The text of a string, held as that string.
3. A piece of a text, which keeps that piece as a copy of its bytes: the positions it answers with are the piece's
   own, so what it holds has to be the piece and not the text behind it.
4. An array of `char` up to its first NUL or its end, copied into a string.
5. The characters at a pointer up to their NUL, copied into a string.

## Parameters

| Parameter | Description |
|---|---|
| `by` | the collator whose equality the searches count by |
| `text` | the text, UTF-8 |

## Complexity

- (1) Constant.
- (2–5) Linear in the length of the text.

## Exceptions

- (1–2) None.
- (3–5) `length_error` when the text is longer than 4294967295 bytes, the most a [string](../../core/string/README.md)
  holds.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator primary{txt::strength::primary};
    string line = "id=7; Résumé: tak";
    txt::collated_text piece(primary, line.as_slice(6));
    println("[{}] {}", piece.text(), piece.find("resume")->at);
}
```

Output:

```text
[Résumé: tak] 0
```

## See also

- [searcher](searcher.md): a pattern weighed the same way
- [sgcl::txt::collated_text](README.md)
