// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for the ASN.1 of sgcl/encoding (asn1.h): Go's encoding/asn1
// asked the same questions, the answers written out as a C++ header the tests
// include:
//
//     go run tools/asn1_oracle.go > tests/encoding/asn1_tests.h
//
// What is asked, by name:
//
//   - the DER Go's Marshal writes of INTEGERs (int64 and big.Int at their
//     edges), BOOLEANs, BIT STRINGs of every length 0 to 17 and 520,
//     OCTET STRINGs, OBJECT IDENTIFIERs, the strings it writes (UTF8String,
//     PrintableString, IA5String, NumericString), UTCTime and
//     GeneralizedTime of instants from 1950 to 2049 and from 0001 to 9999,
//     ENUMERATED, and structures with explicit and implicit tags of the
//     three classes, a SET OF it sorts;
//   - the elements Go's Unmarshal reads of named inputs (the edges of every
//     rule of X.690 a DER reader keeps) and of random structures: a tree of
//     every element, a line each — the class, the tag, c for constructed, and
//     the value of the universal types Go decodes (an INTEGER in decimal, a
//     BOOLEAN, an OID dotted, a BIT STRING's length and bytes, a string's
//     UTF-8, a time as seconds and nanoseconds since 1970, the bytes of the
//     rest) — or "error" when Go refuses any of it.
//
// Where X.690 and Go differ, the test names the input and the reason (a
// UTCTime without seconds or with an offset, a GeneralizedTime with an
// offset, '*' and '&' in a PrintableString, a lone surrogate in a BMPString,
// an arc past 31 bits, all of which Go reads and DER does not have).
package main

import (
	"encoding/asn1"
	"encoding/hex"
	"fmt"
	"math/big"
	"strings"
	"time"
)

type rng struct{ state uint64 }

func (r *rng) next() uint64 {
	r.state += 0x9E3779B97F4A7C15
	z := r.state
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9
	z = (z ^ (z >> 27)) * 0x94D049BB133111EB
	return z ^ (z >> 31)
}

func (r *rng) below(n int) int {
	return int(r.next() % uint64(n))
}

func must(b []byte, err error) []byte {
	if err != nil {
		panic(err)
	}
	return b
}

func cstr(s string) string {
	var sb strings.Builder
	sb.WriteByte('"')
	for i := 0; i < len(s); i++ {
		c := s[i]
		if c == '"' || c == '\\' {
			sb.WriteByte('\\')
			sb.WriteByte(c)
		} else if c >= 0x20 && c < 0x7F && c != '?' {
			sb.WriteByte(c)
		} else {
			fmt.Fprintf(&sb, "\\x%02x\"\"", c)
		}
	}
	sb.WriteByte('"')
	return sb.String()
}

// The tree Go reads of der: a line an element, "error" when any is refused
func tree(der []byte) string {
	var out strings.Builder
	rest, err := walk(der, 0, &out)
	if err != nil || len(rest) != 0 {
		return "error"
	}
	return out.String()
}

func walk(der []byte, depth int, out *strings.Builder) ([]byte, error) {
	var rv asn1.RawValue
	rest, err := asn1.Unmarshal(der, &rv)
	if err != nil {
		return nil, err
	}
	out.WriteString(strings.Repeat(" ", depth))
	fmt.Fprintf(out, "%d:%d", rv.Class, rv.Tag)
	if rv.IsCompound {
		out.WriteString("c\n")
		inner := rv.Bytes
		for len(inner) > 0 {
			inner, err = walk(inner, depth+1, out)
			if err != nil {
				return nil, err
			}
		}
		if rv.Class == 0 && rv.Tag != 16 && rv.Tag != 17 {
			return nil, fmt.Errorf("a constructed universal type other than SEQUENCE and SET")
		}
		return rest, nil
	}
	if rv.Class == 0 && (rv.Tag == 16 || rv.Tag == 17) {
		return nil, fmt.Errorf("a primitive SEQUENCE")
	}
	out.WriteByte(' ')
	full := rv.FullBytes
	if rv.Class != 0 {
		out.WriteString(hex.EncodeToString(rv.Bytes))
		out.WriteByte('\n')
		return rest, nil
	}
	switch rv.Tag {
	case 0:
		return nil, fmt.Errorf("tag 0")
	case asn1.TagBoolean:
		var v bool
		if _, err := asn1.Unmarshal(full, &v); err != nil {
			return nil, err
		}
		fmt.Fprintf(out, "%v", v)
	case asn1.TagInteger:
		v := new(big.Int)
		if _, err := asn1.Unmarshal(full, &v); err != nil {
			return nil, err
		}
		out.WriteString(v.String())
	case asn1.TagEnum:
		var v asn1.Enumerated
		if _, err := asn1.Unmarshal(full, &v); err != nil {
			return nil, err
		}
		fmt.Fprintf(out, "%d", v)
	case asn1.TagOID:
		var v asn1.ObjectIdentifier
		if _, err := asn1.Unmarshal(full, &v); err != nil {
			return nil, err
		}
		out.WriteString(v.String())
	case asn1.TagBitString:
		var v asn1.BitString
		if _, err := asn1.Unmarshal(full, &v); err != nil {
			return nil, err
		}
		fmt.Fprintf(out, "%d %s", v.BitLength, hex.EncodeToString(v.Bytes))
	case asn1.TagNull:
		if len(rv.Bytes) != 0 {
			return nil, fmt.Errorf("NULL with content")
		}
	case asn1.TagUTF8String, asn1.TagPrintableString, asn1.TagIA5String, asn1.TagNumericString, asn1.TagBMPString:
		var v string
		if _, err := asn1.Unmarshal(full, &v); err != nil {
			return nil, err
		}
		out.WriteString(hex.EncodeToString([]byte(v)))
	case asn1.TagUTCTime, asn1.TagGeneralizedTime:
		var v time.Time
		if _, err := asn1.Unmarshal(full, &v); err != nil {
			return nil, err
		}
		fmt.Fprintf(out, "%d.%09d", v.Unix(), v.Nanosecond())
	default:
		out.WriteString(hex.EncodeToString(rv.Bytes))
	}
	out.WriteByte('\n')
	return rest, nil
}

// A random structure Go's Marshal writes: the happy path both read alike
func random(r *rng, depth int) []byte {
	k := r.below(14)
	if depth > 3 && k >= 11 {
		k = r.below(11)
	}
	switch k {
	case 0:
		return must(asn1.Marshal(int64(r.next()) >> r.below(64)))
	case 1:
		n := new(big.Int).SetBytes([]byte(fmt.Sprint(r.next(), r.next())))
		if r.below(2) == 0 {
			n.Neg(n)
		}
		return must(asn1.Marshal(n))
	case 2:
		return must(asn1.Marshal(r.below(2) == 0))
	case 3:
		b := make([]byte, r.below(20))
		for i := range b {
			b[i] = byte(r.next())
		}
		return must(asn1.Marshal(b))
	case 4:
		n := r.below(40)
		b := make([]byte, (n+7)/8)
		for i := range b {
			b[i] = byte(r.next())
		}
		if n%8 != 0 {
			b[len(b)-1] &= byte(0xFF << (8 - n%8))
		}
		return must(asn1.Marshal(asn1.BitString{Bytes: b, BitLength: n}))
	case 5:
		oid := asn1.ObjectIdentifier{r.below(3), r.below(40)}
		for i := r.below(8); i > 0; i-- {
			oid = append(oid, int(r.next()>>(33+r.below(30))))
		}
		return must(asn1.Marshal(oid))
	case 6:
		const letters = "ABCxyz019 '()+,-./:=?"
		var sb strings.Builder
		for i := r.below(12); i > 0; i-- {
			sb.WriteByte(letters[r.below(len(letters))])
		}
		return must(asn1.MarshalWithParams(sb.String(), "printable"))
	case 7:
		words := []string{"zażółć", "gęślą", "jaźń", "日本", "😀", "a", ""}
		return must(asn1.MarshalWithParams(words[r.below(len(words))], "utf8"))
	case 8:
		t := time.Unix(int64(r.next()%2524608000)-631152000, 0).UTC()
		return must(asn1.MarshalWithParams(t, "utc"))
	case 9:
		// the years a datetime holds, 1678 to 2261
		t := time.Unix(int64(r.next()%18400000000)-9200000000, 0).UTC()
		return must(asn1.MarshalWithParams(t, "generalized"))
	case 10:
		return must(asn1.Marshal(asn1.Enumerated(int(int32(r.next())))))
	default:
		var inner []byte
		for i := r.below(5); i > 0; i-- {
			inner = append(inner, random(r, depth+1)...)
		}
		switch k {
		case 11:
			return must(asn1.Marshal(asn1.RawValue{Class: 0, Tag: 16, IsCompound: true, Bytes: inner}))
		case 12:
			return must(asn1.Marshal(asn1.RawValue{Class: 0, Tag: 17, IsCompound: true, Bytes: inner}))
		default:
			return must(asn1.Marshal(asn1.RawValue{Class: 1 + r.below(3), Tag: r.below(40), IsCompound: true, Bytes: inner}))
		}
	}
}

func main() {
	fmt.Println("// Generated by tools/asn1_oracle.go (Go's encoding/asn1): do not edit.")
	fmt.Println("#pragma once")
	fmt.Println("#include <cstdint>")
	fmt.Println("#include <string_view>")
	fmt.Println("namespace asn1_oracle {")

	// INTEGER: the value in decimal and Go's DER
	fmt.Println("struct integer_case { std::string_view decimal; std::string_view der; };")
	fmt.Println("inline constexpr integer_case integers[] = {")
	var ints []*big.Int
	for _, v := range []int64{0, 1, -1, 127, 128, -128, -129, 255, 256, -256, -257, 32767, 32768, -32768, -32769,
		1 << 23, -(1 << 23), 1<<31 - 1, -(1 << 31), 1 << 31, 1<<63 - 1, -(1 << 63)} {
		ints = append(ints, big.NewInt(v))
	}
	for _, s := range []string{"9223372036854775808", "18446744073709551615", "18446744073709551616", "-9223372036854775809",
		"-18446744073709551616", "-18446744073709551617", "340282366920938463463374607431768211455",
		"-340282366920938463463374607431768211456", "123456789012345678901234567890123456789012345678901234567890",
		"-123456789012345678901234567890123456789012345678901234567890"} {
		n, _ := new(big.Int).SetString(s, 10)
		ints = append(ints, n)
	}
	for i := 0; i < 130; i += 7 {
		p := new(big.Int).Lsh(big.NewInt(1), uint(i))
		ints = append(ints, p, new(big.Int).Neg(p), new(big.Int).Sub(p, big.NewInt(1)), new(big.Int).Neg(new(big.Int).Add(p, big.NewInt(1))))
	}
	for _, n := range ints {
		fmt.Printf("    {%s, %s},\n", cstr(n.String()), cstr(hex.EncodeToString(must(asn1.Marshal(n)))))
	}
	fmt.Println("};")

	// ENUMERATED
	fmt.Println("struct enumerated_case { int64_t value; std::string_view der; };")
	fmt.Println("inline constexpr enumerated_case enumerateds[] = {")
	for _, v := range []int{0, 1, -1, 127, 128, 300, -300, 1 << 30} {
		fmt.Printf("    {%d, %s},\n", v, cstr(hex.EncodeToString(must(asn1.Marshal(asn1.Enumerated(v))))))
	}
	fmt.Println("};")

	// BIT STRING: the bytes, the length, Go's DER
	fmt.Println("struct bits_case { std::string_view bytes; size_t length; std::string_view der; };")
	fmt.Println("inline constexpr bits_case bit_strings[] = {")
	full := []byte{0xA5, 0x5A, 0xFF, 0x00, 0x81, 0x7E, 0xC3, 0x3C, 0x99}
	for n := 0; n <= 17; n++ {
		b := append([]byte(nil), full[:(n+7)/8]...)
		if n%8 != 0 {
			b[len(b)-1] &= byte(0xFF << (8 - n%8))
		}
		fmt.Printf("    {%s, %d, %s},\n", cstr(hex.EncodeToString(full[:(n+7)/8])), n, cstr(hex.EncodeToString(must(asn1.Marshal(asn1.BitString{Bytes: b, BitLength: n})))))
	}
	big520 := make([]byte, 65)
	for i := range big520 {
		big520[i] = byte(i * 7)
	}
	fmt.Printf("    {%s, %d, %s},\n", cstr(hex.EncodeToString(big520)), 520, cstr(hex.EncodeToString(must(asn1.Marshal(asn1.BitString{Bytes: big520, BitLength: 520})))))
	fmt.Println("};")

	// OCTET STRING
	fmt.Println("struct bytes_case { std::string_view bytes; std::string_view der; };")
	fmt.Println("inline constexpr bytes_case octet_strings[] = {")
	for _, n := range []int{0, 1, 127, 128, 255, 256, 65535, 65536} {
		b := make([]byte, n)
		for i := range b {
			b[i] = byte(i * 31)
		}
		d := must(asn1.Marshal(b))
		// the header and the first bytes: the rest is the input
		fmt.Printf("    {%s, %s},\n", cstr(fmt.Sprint(n)), cstr(hex.EncodeToString(d[:len(d)-n])))
	}
	fmt.Println("};")

	// OBJECT IDENTIFIER
	fmt.Println("struct oid_case { std::string_view dotted; std::string_view der; };")
	fmt.Println("inline constexpr oid_case oids[] = {")
	for _, o := range []asn1.ObjectIdentifier{{0, 0}, {0, 39}, {1, 0}, {1, 39}, {2, 0}, {2, 39}, {2, 40}, {2, 47}, {2, 48}, {2, 999},
		{1, 2, 840, 113549, 1, 1, 1}, {1, 2, 840, 10045, 2, 1}, {1, 2, 840, 10045, 3, 1, 7}, {1, 3, 132, 0, 34}, {2, 5, 4, 3},
		{1, 3, 6, 1, 4, 1, 311, 21, 20}, {1, 3, 6, 1, 4, 1, 11129, 2, 5, 3}, {2, 16, 840, 1, 101, 3, 4, 2, 1},
		{1, 2, 127}, {1, 2, 128}, {1, 2, 16383}, {1, 2, 16384}, {1, 2, 2097151}, {1, 2, 2097152}, {1, 2, 268435455}, {1, 2, 268435456},
		{1, 2, 2147483647}, {2, 2147483647}} {
		fmt.Printf("    {%s, %s},\n", cstr(o.String()), cstr(hex.EncodeToString(must(asn1.Marshal(o)))))
	}
	fmt.Println("};")

	// the strings
	fmt.Println("struct string_case { int kind; std::string_view text; std::string_view der; };   // 12 utf8, 19 printable, 22 ia5, 18 numeric")
	fmt.Println("inline constexpr string_case strings[] = {")
	for _, c := range []struct {
		kind int
		text string
	}{{12, ""}, {12, "zażółć gęślą jaźń"}, {12, "日本語"}, {12, "😀"}, {12, strings.Repeat("x", 200)},
		{19, ""}, {19, "Hello World"}, {19, "A-Z a-z 0-9 '()+,-./:=?"}, {22, "user@example.com"}, {22, "\x01\x7f"}, {18, "0123 456"}} {
		params := map[int]string{12: "utf8", 19: "printable", 22: "ia5", 18: "numeric"}[c.kind]
		fmt.Printf("    {%d, %s, %s},\n", c.kind, cstr(c.text), cstr(hex.EncodeToString(must(asn1.MarshalWithParams(c.text, params)))))
	}
	fmt.Println("};")

	// the times: the seconds since 1970, 23 UTCTime or 24 GeneralizedTime
	fmt.Println("struct time_case { int64_t seconds; int kind; std::string_view der; };")
	fmt.Println("inline constexpr time_case times[] = {")
	for _, s := range []int64{-631152000, -1, 0, 1, 951782400, 1700000000, 2147483647, 2524607999} {
		t := time.Unix(s, 0).UTC()
		fmt.Printf("    {%d, 23, %s},\n", s, cstr(hex.EncodeToString(must(asn1.MarshalWithParams(t, "utc")))))
	}
	for _, s := range []int64{-9223372036, -5000000000, -631152001, 0, 1700000000, 2524608000, 9223372035} {
		t := time.Unix(s, 0).UTC()
		fmt.Printf("    {%d, 24, %s},\n", s, cstr(hex.EncodeToString(must(asn1.MarshalWithParams(t, "generalized")))))
	}
	fmt.Println("};")

	// structures: a SEQUENCE with explicit and implicit tags of three
	// classes, an optional component left out, a SET OF sorted
	type tagged struct {
		A int
		B int    `asn1:"explicit,tag:0"`
		C int    `asn1:"tag:1"`
		D string `asn1:"utf8,application,tag:5"`
		E bool   `asn1:"private,explicit,tag:31"`
		F []byte `asn1:"tag:200"`
		G []int  `asn1:"optional,explicit,tag:3"`
		H []int  `asn1:"set"`
	}
	v := tagged{A: 5, B: -1, C: 300, D: "żółw", E: true, F: []byte{1, 2, 3}, H: []int{300, 2, 1, 70000, -1}}
	fmt.Printf("inline constexpr std::string_view tagged_structure = %s;\n", cstr(hex.EncodeToString(must(asn1.Marshal(v)))))

	// the named inputs: Go's tree of each, or "error"
	fmt.Println("struct parse_case { std::string_view name; std::string_view der; std::string_view tree; };")
	fmt.Println("inline constexpr parse_case parses[] = {")
	named := []struct{ name, hex string }{
		{"empty sequence", "3000"},
		{"integer zero", "020100"},
		{"integer of no bytes", "0200"},
		{"integer padded with zero", "02020001"},
		{"integer padded with ff", "0202ff80"},
		{"integer 128", "02020080"},
		{"integer -129", "0202ff7f"},
		{"boolean true", "0101ff"},
		{"boolean false", "010100"},
		{"boolean 01", "010101"},
		{"boolean of two bytes", "01020000"},
		{"null", "0500"},
		{"null with content", "050100"},
		{"oid", "06092a864886f70d010101"},
		{"oid empty", "0600"},
		{"oid arc padded", "06032a8001"},
		{"oid cut arc", "06022a86"},
		{"oid arc past 32 bits", "06072a8fffffffff7f"},
		{"bit string empty", "030100"},
		{"bit string no count", "0300"},
		{"bit string count 8", "03020800"},
		{"bit string unused set", "03020701"},
		{"bit string unused clear", "03020780"},
		{"bit string count without bits", "030101"},
		{"octet string", "0403010203"},
		{"utf8", "0c03616263"},
		{"utf8 invalid", "0c02c328"},
		{"printable", "130548656c6c6f"},
		{"printable asterisk", "13012a"},
		{"printable ampersand", "130126"},
		{"printable at", "130140"},
		{"ia5", "1603612e62"},
		{"ia5 high", "160180"},
		{"numeric", "1203312032"},
		{"numeric letter", "120161"},
		{"bmp", "1e0400610105"},
		{"bmp odd", "1e03006100"},
		{"bmp pair", "1e04d83dde00"},
		{"bmp lone surrogate", "1e02d83d"},
		{"utctime", "170d3233313131343232313332305a"},
		{"utctime 1950", "170d3530303130313030303030305a"},
		{"utctime 2049", "170d3439313233313233353935395a"},
		{"utctime no seconds", "170b323331313134323231335a"},
		{"utctime offset", "17113233313131343232313332302b30313030"},
		{"utctime month 13", "170d3233313331343232313332305a"},
		{"utctime feb 29 not leap", "170d3233303232393030303030305a"},
		{"utctime feb 29 leap", "170d3234303232393030303030305a"},
		{"utctime second 60", "170d3233313131343233353936305a"},
		{"utctime no z", "170c323331313134323231333230"},
		{"generalized", "180f32303233313131343232313332305a"},
		{"generalized fraction", "181332303233313131343232313332302e3132335a"},
		{"generalized fraction trailing zero", "181232303233313131343232313332302e31305a"},
		{"generalized fraction no digits", "181032303233313131343232313332302e5a"},
		{"generalized offset", "181332303233313131343232313332302b30313030"},
		{"generalized 9999", "180f39393939313233313233353935395a"},
		{"generalized no seconds", "180d3230323331313134323231335a"},
		{"enumerated", "0a0105"},
		{"enumerated padded", "0a020005"},
		{"high tag 31", "9f1f0100"},
		{"high tag 30 long form", "9f1e0100"},
		{"high tag leading 80", "9f80200100"},
		{"high tag 200", "9f81480100"},
		{"tag 0", "0000"},
		{"long length 0x81 short", "04810100"},
		{"long length padded", "0482000100"},
		{"long length 128", "048180" + strings.Repeat("00", 128)},
		{"length past input", "0405010203"},
		{"indefinite", "30800201050000"},
		{"length ff", "04ff"},
		{"trailing bytes", "05000500"},
		{"constructed octet string", "2406040201020400"},
		{"primitive sequence", "1000"},
		{"constructed integer", "2203020105"},
		{"context primitive", "800105"},
		{"context constructed", "a003020105"},
		{"application", "6103020101"},
		{"private", "c10101"},
		{"nested", "300d300b0609608648016503040201"},
		{"child past parent", "3003020205"},
		{"empty input", ""},
		{"cut header", "30"},
		{"cut length", "3081"},
	}
	for _, c := range named {
		d, err := hex.DecodeString(strings.ReplaceAll(c.hex, " ", ""))
		if err != nil {
			panic(c.name)
		}
		fmt.Printf("    {%s, %s, %s},\n", cstr(c.name), cstr(hex.EncodeToString(d)), cstr(tree(d)))
	}
	fmt.Println("};")

	// random structures Go writes: the trees both read alike
	fmt.Println("struct random_case { std::string_view der; std::string_view tree; };")
	fmt.Println("inline constexpr random_case randoms[] = {")
	r := &rng{state: 20261005}
	for i := 0; i < 1500; i++ {
		d := random(r, 0)
		t := tree(d)
		if t == "error" {
			panic(hex.EncodeToString(d))
		}
		fmt.Printf("    {%s, %s},\n", cstr(hex.EncodeToString(d)), cstr(t))
	}
	fmt.Println("};")
	fmt.Println("}")
}
