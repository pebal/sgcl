[sgcl](../../README.md) › [txt](../README.md) › [collator](README.md)

# sgcl::txt::collator::key

```cpp
vector<byte> key(const string& text) const noexcept;
```

Returns the sort key of a text: a sequence of bytes that compares, byte by byte, exactly the way the collator
compares the texts. It is worth making once for a text that is sorted or looked up many times, and worth storing in
an index beside the text, where the collator need not be called again. ICU's `getSortKey`, Go's `Key`.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Return value

The key, a new [vector](../../core/vector/README.md) of bytes. Two keys compare as the collator compares their texts; two
texts the collator calls equal have equal keys.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Notes

The key belongs to the collator that made it: a key of another language, strength or setting is not comparable to
it. Where a key lives no longer than one pass — a sort, an index builder — [key_to](key_to.md) writes it into a
buffer of the caller's instead of allocating one a word.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator polish{txt::locale("pl")};
    auto zaba = polish.key("żaba");
    auto zamek = polish.key("zamek");
    println("{} bytes; żaba after zamek: {}", zaba.size(), zamek < zaba);
}
```

Output:

```text
36 bytes; żaba after zamek: true
```

## See also

- [key_to](key_to.md): the key into a buffer
- [compare](compare.md): the same order without a key
- [sgcl::txt::collator](README.md)
