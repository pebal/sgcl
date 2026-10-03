[sgcl](../../README.md) › [txt](../README.md) › [value](../value.md)

# sgcl::txt::value::value

```cpp
/*(1)*/ value() noexcept = default;
/*(2)*/ value(std::nullptr_t) noexcept;
/*(3)*/ value(bool v) noexcept;
/*(4)*/ template<class T>
        requires std::integral<T> && (!std::same_as<std::remove_cv_t<T>, bool>)
                 && (!std::same_as<std::remove_cv_t<T>, char>)
                 && (!std::same_as<std::remove_cv_t<T>, char32_t>)
        value(T v) noexcept;
/*(5)*/ template<class T>
        requires std::floating_point<T>
        value(T v) noexcept;
/*(6)*/ value(const string& v) noexcept;
/*(7)*/ value(string&& v) noexcept;
/*(8)*/ template<size_t N>
        value(const char (&v)[N]);
/*(9)*/ template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        value(P v);
/*(10)*/ value(const slice<const char>& v);
/*(11)*/ value(std::initializer_list<value> items) noexcept;
/*(12)*/ value(const vector<value>& items) noexcept;
/*(13)*/ value(char) = delete;
/*(14)*/ value(char32_t) = delete;
```

Makes a value. None of them is explicit, so a value is written as the thing it holds wherever one is wanted.

- (1–2) Nothing, `value_kind::none`: what a name nobody gave answers to, written as nothing.
3. A truth.
4. A whole number, held as the widest one, `long long`; an unsigned one above `LLONG_MAX` is held as the real
   number nearest it, `value_kind::real`, rather than wrapped round into a negative one.
5. A real number, held as a `double`.
- (6–7) The text, the string shared and not copied.
8. A C text: the array up to its first NUL or its end, copied into a string.
9. A C text: the characters up to the NUL, copied into a string.
10. The characters of the slice, copied into a string.
- (11–12) A list of the values, in order. `value v{"one"}` is therefore a list of one text, where `value v("one")` is
  the text; a list nested in an [object](../object.md) needs no type named, `{"tags", {"one", "two"}}`.
- (13–14) Deleted: a character is a byte of UTF-8, not a number and not a truth, and without these it would become
  `true`. Text is written as text, `"A"`, and a number as a number, `int('A')`.

## Parameters

| Parameter | Description |
|---|---|
| `v` | what the value holds |
| `items` | the elements of the list |

## Complexity

- (1–7) Constant.
- (8–10) Linear in the length of the text.
- (11–12) Linear in the number of elements.

## Exceptions

- (8–10) `length_error` when the text passes 4 GiB, the most a string holds.
- Otherwise none.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value one("one");
    txt::value listed{"one"};  // a list of one text
    txt::value values[] = {nullptr, true, 42, 2.5, string("text"), {1, 2}};
    println("{} {}", int(one.kind()), int(listed.kind()));
    for (auto& v : values) {
        print("{} ", v);
    }
    println();
    return 0;
}
```

Output:

```text
4 5
 true 42 2.5 text [1, 2] 
```

## See also

- [list](../list.md), [object](../object.md): a list and a mapping written as data
- [value_kind](../value_kind.md)
- [sgcl::txt::value](../value.md)
