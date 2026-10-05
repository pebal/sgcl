[sgcl](../../README.md) › [core](../README.md)

# sgcl::slice\<T\>

```cpp
#include "sgcl/core/slice.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class slice;

    using string_slice = slice<const char>;   // sgcl/core/string.h
}
```

**Requires [rooted](../rooted/README.md) outside a stack or a managed object.**

`sgcl::slice<T>` is the elements `[begin, end)` of some contiguous storage and the managed object they lie in, kept
alive by the slice for as long as the slice exists: what a slice is in Go (a piece of the array that shares it and
holds it), and what `std::span` and `std::string_view` are not (a range with no duty to keep its memory). Three
words: the owner, a `tracked_ptr` to the object — a [string](../string/README.md), the buffer of a [vector](../vector/README.md), the
block of a [buffered_reader](../../io/buffered_reader/README.md) — and two raw pointers into it. A slice of unmanaged memory (a stack
array, a `std::vector`, a `std::span`) has no owner: its word is null, and the slice promises what a span does, the
memory valid for the call. Which of the two a slice is follows from where the memory comes from, not from a choice:
a string, a vector, a reader's block hand out slices with the owner set (`s.as_slice()`, `v.as_slice()`, a line of
`read_line`); a raw pointer or a `std` container give one without. The owner is given by whoever knows it, never
guessed from an address (a pointer into an object finds the object only within its first page).

What it is for, with an owner: a piece of a string with no copy — a token, a field, a line, the pieces of `split` —
kept in a container or a managed object as it is, the source alive for as long as any piece is; a line of a file
handed out by a reader without an allocation, valid after the reader has moved on to the next block; a fragment of
a buffer given to an asynchronous `read`, the buffer rooted by the argument while the task waits. Without an owner:
what a `std::span` is for, one type for both. `slice<const char>` is text and has the read interface of a string
([mixin::text](../mixin/text/README.md): `find`, `starts_with`, `compare`, …, and `trim`, `substr` of its own);
`slice<byte>` is a buffer to read into, `slice<const byte>` data to write; `slice<T>` of anything else is a span
with an owner.

What it costs: a slice without an owner is three word stores — no barrier, no registration of the thread, the price
of a span. A slice with an owner is a `tracked_ptr`'s copy: the write barrier, and the registration of the thread's
stack the first time a managed word lands on it. The rule the two paths keep is that a non-null owner never lands
on a stack the collector does not know: the constructor from an owner, and the copy and the assignment from an
owned slice, register; the paths without an owner skip it.

## Rules

- A slice without an owner is no exception, since one may be assigned to it; what goes into unmanaged memory is a
  `std::span` ([operator std::span](operator_conv.md)).
- The owner is explicit: `slice(owner, first, last)` with the object the elements lie in; in a debug build the
  constructor asserts that `[first, last)` lies in it. A slice of a `string` or a `vector` comes from the container
  (`as_slice`), which knows its object.
- `data()` is not terminated: a slice is a range, not a C string; `str()` for a `std::string`, `string(s)` for a
  string. A text slice made from a literal or another array of `const` characters ends at its first NUL or the
  array's end, so `string_slice s = "ab"` holds two characters, not the terminator.
- A slice's elements are `T`: a `slice<int>` writes through, a `slice<const int>` does not; `slice<T>` converts to
  `slice<const T>`, the owner carried over.
- A text is bytes where bytes are taken: `slice<const byte>` is made, implicitly, from a `string` or a text slice
  (the owner carried over), a `std::string_view`, a literal or an array of `char` (up to its first NUL, never past
  its end: no terminator), and an array of `unsigned char` or a `std::array<uint8_t, N>` (all of it). So a parameter
  of data takes a text as it is: `hex::encode("abc")`, `gcm.seal(nonce, text, header)`,
  `hmac_sha256::of(message, "key")`. Nothing goes the other way: bytes are not text, and a `slice<byte>` to write
  into is made from nothing of this. A pointer is not taken, having no length. Where one name takes a text
  (`const string&`) and bytes (`const slice<const byte>&`), a literal and a `std::string_view` convert to both and
  would be ambiguous: such a set adds an exact overload for them, a template over a literal or a
  `std::string_view`, as the library's do.
- The elements a slice with an owner points at are the memory of that moment: a reader reuses its block for the
  next lines, a vector overwrites its buffer, so a slice kept across such a change reads what was written since —
  alive and well-formed, not what it read before. A slice kept for its text is copied first (`string(line)`).
- Inside a managed object, the raw `begin` and `end` are words the collector's pointer map traces like any
  address-holding word, so the destructor nulls them: a container slot a slice was destroyed in keeps nothing alive.
- Threads share a slice the way they share a `tracked_ptr` ([The rules](../README.md#the-rules), 6): a slice variable
  that one thread replaces while others read it needs the program's synchronization (three words: not an atomic).

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements, `const` for a slice that does not write. A character type gives the text interface to `slice<const T>`; `byte` and `const byte` are the buffers of the library. |

## Member types

| Type | Definition |
|---|---|
| `element_type` | `T` |
| `value_type` | `std::remove_cv_t<T>` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `pointer` | `T*` |
| `const_pointer` | `const T*` |
| `reference` | `T&` |
| `const_reference` | `const T&` |
| `iterator` | `T*` |
| `const_iterator` | `const T*` |
| `reverse_iterator` | `std::reverse_iterator<iterator>` |
| `const_reverse_iterator` | `std::reverse_iterator<const_iterator>` |

## Member objects

| Member | Description |
|---|---|
| `npos` | `static constexpr size_type`, `size_type(-1)`: "to the end" as a count, "not found" as a position |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](slice.md) | constructs a slice of an owner, of unmanaged memory, of a container or a text |
| `(destructor)` | nulls the two raw pointers; the owner is released as a `tracked_ptr` is |
| [operator=](operator_assign.md) | assigns another slice |

#### Element access

| Function | Description |
|---|---|
| [operator[]](operator_at.md) | access the element at a position |
| [front](front.md) | access the first element |
| [back](back.md) | access the last element |
| [data](data.md) | the elements as a plain pointer |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the beginning |
| [end, cend](end.md) | an iterator to the end |
| [rbegin, crbegin](rbegin.md) | a reverse iterator to the beginning |
| [rend, crend](rend.md) | a reverse iterator to the end |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the slice is empty |
| [size](size.md) | the number of elements |
| [size_bytes](size_bytes.md) | the size of the elements in bytes |

#### Observers

| Function | Description |
|---|---|
| [owner](owner.md) | the managed object the elements lie in, null for unmanaged memory |
| [owned](owned.md) | checks whether the slice has an owner |

#### Subslices

| Function | Description |
|---|---|
| [subslice](subslice.md) | the elements `[pos, pos + n)`, a slice of the same owner |
| [subspan](subspan.md) | the same, `std::span`'s name |
| [first](first.md) | the first `n` elements |
| [last](last.md) | the last `n` elements |
| [as_slice](as_slice.md) | the slice itself, the question every container with a buffer answers |

#### Modifiers

| Function | Description |
|---|---|
| [remove_prefix](remove_prefix.md) | narrows the slice from the start |
| [remove_suffix](remove_suffix.md) | narrows the slice from the end |
| [swap](swap.md) | swaps two slices |

#### Text

The members of `slice<const CharT>` for a character type `CharT`: each piece is a slice of the same owner, nothing
copied.

| Function | Description |
|---|---|
| [substr](substr.md) | the characters `[pos, pos + n)`, `std::string_view`'s name for `subslice` |
| [trim](trim.md) | the text without white space, or the given characters, at both ends |
| [trim_left](trim_left.md) | the same at the start |
| [trim_right](trim_right.md) | the same at the end |
| [trim_prefix](trim_prefix.md) | the text without a prefix, when it is there |
| [trim_suffix](trim_suffix.md) | the text without a suffix, when it is there |
| [contains](contains.md) | checks for a substring or a character; for other elements, for an element |

#### Conversions

| Function | Description |
|---|---|
| [operator std::span](operator_conv.md) | the elements as a `std::span`, without the owner |

#### From mixin::text

The read side of `std::string_view`, for `slice<const CharT>` ([mixin::text](../mixin/text/README.md)).

| Function | Description |
|---|---|
| `view`, `operator view_type` | the characters as a `std::basic_string_view` |
| `str` | a `std::basic_string` of the characters |
| `length`, `at` | the number of characters, the character at a position with bounds checking |
| `compare`, `starts_with`, `ends_with`, `equal_fold` | compare the text |
| `find`, `rfind`, `find_first_of`, `find_last_of`, `find_first_not_of`, `find_last_not_of` | search the text |
| `copy` | copies characters out |
| `runes`, `rune_count`, `decode`, `is_valid_utf8` | the code points of a UTF-8 text ([utf8](../utf8/README.md)) |

#### From mixin::enumerable

The questions asked of the elements, carried by every container of the library
([mixin::enumerable](../mixin/enumerable/README.md)).

| Function | Description |
|---|---|
| `index_of` | the position of the first element equal to a value |
| `last_index_of` | the position of the last element equal to a value |
| `find_if` | a pointer to the first element the predicate accepts |
| `find_index` | the position of the first element the predicate accepts |
| `exists` | checks whether the predicate accepts some element |
| `all` | checks whether the predicate accepts every element |
| `count_of` | the number of elements the predicate accepts |
| `min`, `max` | the smallest, the largest element |
| `for_each` | calls a function with every element |

#### From mixin::ordered

The order of the elements ([mixin::ordered](../mixin/ordered/README.md)); the sorts only for a slice whose elements are not
`const`.

| Function | Description |
|---|---|
| `sort` | sorts the elements |
| `sort_by` | sorts the elements by a projection |
| `stable_sort` | sorts the elements, keeping the order of equal ones |
| `is_sorted` | checks whether the elements are sorted |
| `binary_search` | checks whether a sorted slice holds a value |
| `lower_bound`, `upper_bound` | the first element not less than, greater than a value, in a sorted slice |
| `sorted_index_of` | the position of a value in a sorted slice |

#### From mixin::sequence

The writes over every element, for a slice whose elements are not `const` ([mixin::sequence](../mixin/sequence/README.md)).

| Function | Description |
|---|---|
| `fill` | assigns a value to every element |
| `reverse` | reverses the order of the elements |

## Non-member functions

| Function | Description |
|---|---|
| [as_bytes, as_writable_bytes](as_bytes.md) | the bytes of the elements, a slice of the same owner |
| `operator==`, `operator<=>` | compare the elements lexicographically ([mixin::equatable](../mixin/equatable/README.md), [mixin::comparable](../mixin/comparable/README.md)); a text slice also with a `std::string_view`, a literal, a C string ([mixin::text](../mixin/text/README.md)) and a `string` |
| `operator<<` | writes a text slice to a `std::basic_ostream` |

## Deduction guides

```cpp
template<class T>
slice(T*, T*) -> slice<T>;
template<class T>
slice(T*, size_t) -> slice<T>;
template<class T, size_t N>
slice(T (&)[N]) -> slice<T>;
template<class T, size_t N>
slice(std::array<T, N>&) -> slice<T>;
template<class T, size_t N>
slice(const std::array<T, N>&) -> slice<const T>;
template<class T, class A>
slice(std::vector<T, A>&) -> slice<T>;
template<class T, class A>
slice(const std::vector<T, A>&) -> slice<const T>;
template<class T>
slice(std::span<T>) -> slice<T>;
```

## Specializations

`std::hash<slice<const CharT>>`, `std::equal_to<slice<const CharT>>` and `std::less<slice<const CharT>>` are
transparent: a slice hashes as a `string` of the same characters does, and a map keyed by slices is looked up by a
`string`, a `std::string_view` or a literal without a copy.

## Complexity

Every member function is constant, but the text's searches and trims, linear in the characters they read, and the
mixins' questions, linear in the elements (logarithmic for the binary searches).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// A tokenizer whose tokens are slices of the text: nothing copied, the text alive for as long as
// any token is, wherever the tokens go
struct Token {
    string_slice text;
    int line;
};

vector<Token> tokenize(const string& source) {
    vector<Token> tokens;
    int line = 1;
    for (auto raw : source.split('\n')) {  // each piece a slice of source
        for (auto word : string(raw).fields()) {  // a string of the line: one allocation per line
            tokens.push_back({word, line});
        }
        ++line;
    }
    return tokens;
}

int main() {
    vector<Token> tokens;
    {
        string source = "let x = 1\nlet y = x + 2";  // dies at the brace, as a variable
        tokens = tokenize(source);
    }
    collector::force_collect();  // optional, to show the result at once
    for (auto& t : tokens) {  // the texts live on: each slice holds its line
        println("{}: {}", t.line, t.text);
    }
    println("{} tokens, {}", tokens.size(), tokens[0].text.owned() ? "owned" : "unowned");
}
```

Output:

```text
1: let
1: x
1: =
1: 1
2: let
2: y
2: =
2: x
2: +
2: 2
10 tokens, owned
```

## See also

- [string](../string/README.md): `as_slice`, `split` and `fields` hand out slices; a string of a slice
- [vector](../vector/README.md): `as_slice` over the buffer
- [buffered_reader](../../io/buffered_reader/README.md): lines as slices of the block; [reader](../../io/reader/README.md), [writer](../../io/writer/README.md): `read` and `write`
  take slices
- [utf8, unicode, runes](../utf8/README.md): the code points of a text slice
- [tracked_ptr](../tracked_ptr/README.md): the owner's word and its barrier
- [README: The rules](../README.md#the-rules)
