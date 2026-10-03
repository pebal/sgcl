[sgcl](../../../README.md) › [hash](../../README.md) › mixin › [hasher](../hasher.md)

# sgcl::hash::mixin::hasher\<Derived\>::update

```cpp
/*(1)*/ void update(const string& text) noexcept;
/*(2)*/ void update(const slice<const char>& text) noexcept;
/*(3)*/ void update(const slice<char>& text) noexcept;
/*(4)*/ template<size_t N>
        void update(const char (&text)[N]) noexcept;
/*(5)*/ template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        void update(P text) noexcept;
/*(6)*/ void update(std::string_view text) noexcept;
/*(7)*/ template<size_t N>
        void update(const array<byte, N>& digest) noexcept;
/*(8)*/ template<class S>
        requires std::same_as<S, std::span<byte>> || std::same_as<S, std::span<const byte>>
        void update(S bytes) noexcept;
```

Hashes a text, the digest of another hasher or a `std::span` of bytes after what the hasher took before, by handing
its bytes to the class's own `update(const slice<const byte>&)` ([crc32::update](../../crc32/update.md) and the
others) where they lie, with no string made and no owner copied.

1. The bytes of a [string](../../../core/string.md).
2. The bytes of a slice of characters: a part of a string, a line of a reader.
3. The same for a slice of characters that are not `const`.
4. A literal or an array of `char`, up to its first NUL or its end, whichever comes first: a buffer with no NUL in
   it is never read past, and a literal with a NUL inside is cut there, as a `std::string_view` made from it would
   be. The bytes of such a literal go in as a `std::string_view` with its length, or as bytes. This overload is
   also what keeps a literal from being ambiguous between the string and the slice.
5. A C string, `const char*` or `char*`, up to its NUL; a template of its own so that an array never decays to it.
6. A `std::string_view`, and a `std::string` through it.
7. A digest, `array<byte, N>`, so that one hash goes into another.
8. A `std::span` of bytes, `const` or not.

- (1–6) A text is hashed as its bytes, UTF-8 as they lie, without normalizing: two texts that read the same but are
  written differently hash differently, and [txt::hash_normalized](../../../txt/normalize.md) is the hash for that.

The bytes of a `vector<byte>`, an array of `uint8_t` and whatever else a `slice<const byte>` is made from go
straight to the class's own `update`. Go's `h.Write(p)` takes only bytes, and its error is always nil; here nothing
fails.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to hash |
| `digest` | the digest of another hasher |
| `bytes` | the bytes to hash |

## Return value

None.

## Complexity

Linear in the number of bytes hashed; (4) and (5) also look for the NUL.

## Exceptions

None.

## Notes

A text goes to the class's `update` as a slice with no owner: three words, no barrier and no registration of the
thread. That is sound because the caller holds the text for the whole call and no hasher keeps a slice after it; it
matters because a copy of the string's owner would cost a short key more than its hash.

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

#include <string_view>

using namespace sgcl;

int main() {
    hash::crc32 h;
    h.update("hello, ");
    h.update(string("world"));
    println("{:08x}", h.value());

    // an array of char ends at its first NUL; a string_view keeps its length
    char buffer[16] = "abc";
    hash::crc32 a, b, c;
    a.update(buffer);
    b.update("abc\0def");
    c.update(std::string_view("abc\0def", 7));
    println("{} {}", a.value() == b.value(), b.value() == c.value());

    // one hash into another
    hash::xxh3_64 outer;
    outer.update(h.digest());
    println("{:016x}", outer.value());
}
```

Output:

```text
ffab723a
true false
ef1cb5ed640b61b9
```

## See also

- [of](of.md): the hash of the same forms in one call
- [copy_from](copy_from.md): a stream read into the hasher
- [sgcl::hash::mixin::hasher\<Derived\>](../hasher.md)
