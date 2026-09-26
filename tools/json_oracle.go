// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for the JSON of sgcl/encoding (json.h): Go's encoding/json/v2
// and jsontext (whose defaults are the ones json.h keeps) and strconv
// asked the same questions, the answers written out as a C++ header the
// tests include. Run it from the root of the tree:
//
//     go run tools/json_oracle.go > tests/encoding/json_tests.h
//
// What is asked, by name, because an oracle checks only what it is asked:
//
//   - doubles written: the named ones (the powers of ten from 1e-324 to
//     1e308, the edges of the fixed layout, the smallest subnormal and
//     normal, the largest, 0.1 + 0.2, 2^53 and its neighbours) in full,
//     and a million random bit patterns as an FNV-1a hash of their texts;
//   - floats written the same way, apart: Go's float32 in a structure;
//   - literals read, each both as a double and as a float (strconv's
//     ParseFloat with 64 and 32 bits, each rounded once): the named hard
//     ones in full, and half a million random literals — up to 900
//     digits, exponents to +-340 — as a hash of the bits;
//   - strings written, raw and with escape_html, against Marshal with
//     invalid UTF-8 allowed;
//   - every file of JSONTestSuite's test_parsing (from
//     ~/Programming/oracles/JSONTestSuite): whether jsontext takes it as
//     a text of JSON (the reader's verdict), whether Unmarshal into any
//     takes it (json::parse's: a number past a double's range is refused
//     there, as here), the hash of its tokens, and the hash of the value
//     written back compact and indented.
//
// The literals and the bit patterns come from splitmix64, which the test
// writes the same way. A value written back is the canonical text of
// json.h: an integer literal as the integer (or as itself past uint64),
// "-0" as -0, any other number as the double written by Go, the strings
// by Marshal, the members in their order; indented by json.Indent.
package main

import (
	"bytes"
	"encoding/json"
	"encoding/json/jsontext"
	jsonv2 "encoding/json/v2"
	"fmt"
	"io"
	"math"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
)

type rng struct{ state uint64 }

func (r *rng) next() uint64 {
	r.state += 0x9E3779B97F4A7C15
	z := r.state
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9
	z = (z ^ (z >> 27)) * 0x94D049BB133111EB
	return z ^ (z >> 31)
}

type fnv struct{ h uint64 }

func newFnv() *fnv { return &fnv{0xcbf29ce484222325} }

func (f *fnv) add(b []byte) {
	for _, c := range b {
		f.h ^= uint64(c)
		f.h *= 0x100000001b3
	}
}

func (f *fnv) addString(s string) { f.add([]byte(s)) }

func quote(b []byte) string {
	var sb strings.Builder
	sb.WriteByte('"')
	for _, c := range b {
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

func marshalFloat(f float64, bits int) string {
	var b []byte
	var err error
	if bits == 32 {
		b, err = jsonv2.Marshal(float32(f))
	} else {
		b, err = jsonv2.Marshal(f)
	}
	if err != nil {
		panic(err)
	}
	return string(b)
}

// A random literal of JSON's grammar: the generator of tests/encoding/json.cpp
func literal(r *rng) string {
	var sb strings.Builder
	if r.next()&1 == 1 {
		sb.WriteByte('-')
	}
	kind := r.next() % 16
	var digits int
	switch {
	case kind < 12:
		digits = 1 + int(r.next()%20)
	case kind < 15:
		digits = 1 + int(r.next()%40)
	default:
		digits = 400 + int(r.next()%500)
	}
	intDigits := int(r.next() % uint64(digits+1))
	if intDigits == 0 {
		sb.WriteByte('0')
	} else {
		sb.WriteByte(byte('1' + r.next()%9))
		for i := 1; i < intDigits; i++ {
			sb.WriteByte(byte('0' + r.next()%10))
		}
	}
	if frac := digits - intDigits; frac > 0 {
		sb.WriteByte('.')
		for i := 0; i < frac; i++ {
			sb.WriteByte(byte('0' + r.next()%10))
		}
	}
	if r.next()%3 != 0 {
		sb.WriteByte('e')
		switch r.next() % 3 {
		case 0:
			sb.WriteByte('-')
		case 1:
			sb.WriteByte('+')
		}
		sb.WriteString(strconv.Itoa(int(r.next() % 341)))
	}
	return sb.String()
}

// The bits a literal reads as, or all ones when it is past the range
func parsed(lit string, bits int) uint64 {
	f, err := strconv.ParseFloat(lit, bits)
	if err != nil {
		if math.IsInf(f, 0) {
			return math.MaxUint64
		}
		panic(err)
	}
	if bits == 32 {
		return uint64(math.Float32bits(float32(f)))
	}
	return math.Float64bits(f)
}

// The number as json.h writes it back
func canonicalNumber(lit string) string {
	if !strings.ContainsAny(lit, ".eE") {
		if lit == "-0" {
			return "-0"
		}
		if i, err := strconv.ParseInt(lit, 10, 64); err == nil {
			return strconv.FormatInt(i, 10)
		}
		if u, err := strconv.ParseUint(lit, 10, 64); err == nil {
			return strconv.FormatUint(u, 10)
		}
		return lit
	}
	f, err := strconv.ParseFloat(lit, 64)
	if err != nil {
		panic(err)
	}
	return marshalFloat(f, 64)
}

// The value of a text as json.h writes it compact, when Unmarshal takes it
func canonical(data []byte) (string, bool) {
	var v any
	if jsonv2.Unmarshal(data, &v) != nil {
		return "", false
	}
	dec := jsontext.NewDecoder(bytes.NewReader(data))
	type level struct {
		object, afterKey bool
		count            int
	}
	var stack []level
	var out strings.Builder
	sep := func() {
		if len(stack) == 0 {
			return
		}
		l := &stack[len(stack)-1]
		if l.afterKey {
			l.afterKey = false
			return
		}
		if l.count > 0 {
			out.WriteByte(',')
		}
		l.count++
	}
	for {
		tok, err := dec.ReadToken()
		if err == io.EOF {
			break
		}
		if err != nil {
			panic(err)
		}
		switch k := tok.Kind(); k {
		case '{', '[':
			sep()
			out.WriteByte(byte(k))
			stack = append(stack, level{object: k == '{'})
		case '}', ']':
			stack = stack[:len(stack)-1]
			out.WriteByte(byte(k))
		case '"':
			b, err := jsonv2.Marshal(tok.String())
			if err != nil {
				panic(err)
			}
			if len(stack) > 0 && stack[len(stack)-1].object && !stack[len(stack)-1].afterKey {
				sep()
				out.Write(b)
				out.WriteByte(':')
				stack[len(stack)-1].afterKey = true
			} else {
				sep()
				out.Write(b)
			}
		case '0':
			sep()
			out.WriteString(canonicalNumber(tok.String()))
		default:
			sep()
			out.WriteString(tok.String())
		}
	}
	return out.String(), true
}

// The tokens as the test lists them: a letter and the text, a line each
func tokens(data []byte) (uint64, bool) {
	if !jsontext.Value(data).IsValid() {
		return 0, false
	}
	dec := jsontext.NewDecoder(bytes.NewReader(data))
	type level struct{ object, afterKey bool }
	var stack []level
	h := newFnv()
	for {
		tok, err := dec.ReadToken()
		if err == io.EOF {
			break
		}
		if err != nil {
			panic(err)
		}
		inKey := false
		if len(stack) > 0 && stack[len(stack)-1].object {
			l := &stack[len(stack)-1]
			if l.afterKey {
				l.afterKey = false
			} else if tok.Kind() == '"' {
				inKey = true
				l.afterKey = true
			}
		}
		var letter byte
		switch k := tok.Kind(); k {
		case '{', '[':
			letter = byte(k)
			stack = append(stack, level{object: k == '{'})
		case '}', ']':
			letter = byte(k)
			stack = stack[:len(stack)-1]
		case '"':
			letter = 's'
			if inKey {
				letter = 'k'
			}
		case '0':
			letter = 'n'
		case 't':
			letter = 't'
		case 'f':
			letter = 'f'
		case 'n':
			letter = 'z'
		}
		h.add([]byte{letter})
		if letter == '{' || letter == '}' || letter == '[' || letter == ']' {
			h.add([]byte{letter})
		} else {
			h.addString(tok.String())
		}
		h.add([]byte{'\n'})
	}
	return h.h, true
}

func main() {
	w := os.Stdout
	fmt.Fprintln(w, "// Generated by tools/json_oracle.go (Go's encoding/json/v2, jsontext and strconv): do not edit.")
	fmt.Fprintln(w, "#pragma once")
	fmt.Fprintln(w, "#include <cstdint>")
	fmt.Fprintln(w, "#include <string_view>")
	fmt.Fprintln(w, "namespace json_oracle {")

	// --- doubles and floats written ---
	var named []float64
	for e := -324; e <= 308; e++ {
		f, _ := strconv.ParseFloat("1e"+strconv.Itoa(e), 64)
		named = append(named, f)
	}
	for _, s := range []string{"1e-7", "9.999999e-7", "1e-6", "1.0000001e-6", "1e20", "9.99999999999999e20", "1e21", "1.0000000000000001e21", "123456789012345678901", "5e-324", "2.2250738585072014e-308", "2.225073858507201e-308", "1.7976931348623157e308", "0.1", "0.30000000000000004", "9007199254740991", "9007199254740992", "9007199254740993", "-0", "0", "1", "-1", "0.5", "100", "1e100", "123.456", "-1.5e-10", "3.4028234663852886e38", "1.401298464324817e-45", "1.1754943508222875e-38", "16777216", "16777217", "0.000001", "0.0000001"} {
		f, _ := strconv.ParseFloat(s, 64)
		named = append(named, f)
	}
	named = append(named, 0.1+0.2, math.Copysign(0, -1))
	fmt.Fprintln(w, "struct written { uint64_t bits; std::string_view text; };")
	fmt.Fprintln(w, "inline constexpr written doubles_written[] = {")
	for _, f := range named {
		fmt.Fprintf(w, "    {0x%016xull, %s},\n", math.Float64bits(f), quote([]byte(marshalFloat(f, 64))))
	}
	fmt.Fprintln(w, "};")
	fmt.Fprintln(w, "inline constexpr written floats_written[] = {")
	for _, f := range named {
		f32 := float32(f)
		if math.IsInf(float64(f32), 0) {
			continue
		}
		fmt.Fprintf(w, "    {0x%08xull, %s},\n", math.Float32bits(f32), quote([]byte(marshalFloat(float64(f32), 32))))
	}
	fmt.Fprintln(w, "};")
	{
		r := &rng{12345}
		h := newFnv()
		for i := 0; i < 1000000; {
			f := math.Float64frombits(r.next())
			if math.IsNaN(f) || math.IsInf(f, 0) {
				continue
			}
			h.addString(marshalFloat(f, 64))
			h.add([]byte{'\n'})
			i++
		}
		fmt.Fprintf(w, "inline constexpr uint64_t random_doubles_hash = 0x%016xull;   // a million, seed 12345\n", h.h)
		r = &rng{54321}
		h = newFnv()
		for i := 0; i < 1000000; {
			f := math.Float32frombits(uint32(r.next() >> 32))
			if math.IsNaN(float64(f)) || math.IsInf(float64(f), 0) {
				continue
			}
			h.addString(marshalFloat(float64(f), 32))
			h.add([]byte{'\n'})
			i++
		}
		fmt.Fprintf(w, "inline constexpr uint64_t random_floats_hash = 0x%016xull;    // a million, seed 54321\n", h.h)
	}

	// --- literals read ---
	hard := []string{
		"0", "-0", "1", "0.1", "1e400", "-1e400", "1e-400", "-1e-400", "1e308", "1.7976931348623157e308", "1.7976931348623158e308", "1.7976931348623159e308",
		"2.2250738585072011e-308", "2.2250738585072012e-308", "4.9406564584124654e-324", "2.4703282292062327e-324", "2.4703282292062328e-324", "5e-324", "3e-324",
		"9007199254740993", "9007199254740993.0000000000000000001", "9007199254740992.9999999999999999999", "0.30000000000000004", "1.00000005960464477539062499", "1.000000059604644775390625",
		"1.00000005960464477539062501", "3.4028235e38", "3.4028235677973366e38", "3.4028236e38", "1.4e-45", "7e-46", "7.1e-46", "1e-45", "1.17549435e-38",
		"0e99999999999999999999", "1e-99999999999999999999", "123456789012345678901234567890e-10", "0.000000000000000000000000000000000000000000001",
		"179769313486231580793728971405303415079934132710037826936173778980444968292764750946649017977587207096330286416692887910946555547851940402630657488671505820681908902000708383676273854845817711531764475730270069855571366959622842914819860834936475292719074168444365510704342711559699508093042880177904174497791.9999999999999999999999999999999999999999999999999999999999999999999999",
		"2.47032822920623272088284396434110686182529901307162382212792841250337753635104375932649918180817996189898282347722858865463328355177969898199387398005390939063150356595155702263922908583924491051844359318028499365361525003193704576782492193656236698636584807570015857692699037063119282795585513329278343384093519780155312465972635795746227664652728272200563740064854999770965994704540208281662262378573934507363390079677619305775067401763246736009689513405355374585166611342237666786041621596804619144672918403005300575308490487653917113865916462395249126236538818796362393732804238910186723484976682350898633885879256283027559956575244555072551893136908362547791869486679949683240497058210285131854513962138377228261454376934125320985913276672363281255",
		"1e23", "8.533e+68", "4.1006e-184", "9.998e+307", "9.9538452227e-280", "6.47660115e-260", "7.4e+47", "5.92e+48", "7.35e+66", "8.32116e+55",
	}
	fmt.Fprintln(w, "struct read { std::string_view literal; uint64_t f64; uint64_t f32; };   // all ones: past the range")
	fmt.Fprintln(w, "inline constexpr read literals_read[] = {")
	for _, lit := range hard {
		fmt.Fprintf(w, "    {%s, 0x%016xull, 0x%016xull},\n", quote([]byte(lit)), parsed(lit, 64), parsed(lit, 32))
	}
	fmt.Fprintln(w, "};")
	{
		r := &rng{777}
		h := newFnv()
		var b [8]byte
		for i := 0; i < 500000; i++ {
			lit := literal(r)
			for _, bits := range []int{64, 32} {
				v := parsed(lit, bits)
				for k := 0; k < 8; k++ {
					b[k] = byte(v >> (8 * k))
				}
				h.add(b[:])
			}
		}
		fmt.Fprintf(w, "inline constexpr uint64_t random_literals_hash = 0x%016xull;   // half a million, seed 777\n", h.h)
	}

	// --- strings written ---
	strs := []string{"", "abc", "a\"b\\c", "\x00\x01\x07\b\t\n\v\f\r\x1b\x1f", "\x7f", "<a href='x'>&amp;</a>", "\u2028\u2029", "zażółć gęślą jaźń", "€😀", "\xff", "a\xc3", "\xc3(", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xc0\xaf", "\xe0\x80\xaf", "\xf0\x9f\x98", "ok\xe2\x82\xacok", "/"}
	fmt.Fprintln(w, "struct string_written { std::string_view raw; std::string_view plain; std::string_view html; };")
	fmt.Fprintln(w, "inline constexpr string_written strings_written[] = {")
	for _, s := range strs {
		plain, err := jsonv2.Marshal(s, jsontext.AllowInvalidUTF8(true))
		if err != nil {
			panic(err)
		}
		html, err := jsonv2.Marshal(s, jsontext.AllowInvalidUTF8(true), jsontext.EscapeForHTML(true))
		if err != nil {
			panic(err)
		}
		fmt.Fprintf(w, "    {std::string_view(%s, %d), std::string_view(%s, %d), std::string_view(%s, %d)},\n", quote([]byte(s)), len(s), quote(plain), len(plain), quote(html), len(html))
	}
	fmt.Fprintln(w, "};")

	// --- a structure marshalled (v1, which sorts the keys of a map) ---
	{
		type goRecord struct {
			Name     string         `json:"name"`
			Count    int32          `json:"count"`
			Ratio    float32        `json:"ratio"`
			Big      float64        `json:"big"`
			List     []int64        `json:"list"`
			Scores   map[string]int `json:"scores"`
			ByNumber map[int]string `json:"by_number"`
			Nothing  *string        `json:"nothing"`
			Floats   []float32      `json:"floats"`
			Ok       bool           `json:"ok"`
		}
		r := goRecord{Name: "record", Count: -7, Ratio: 0.3, Big: 12345678901234567890.0, List: []int64{1, -2, 3},
			Scores: map[string]int{"zeta": 1, "alpha": 2, "mid": 3}, ByNumber: map[int]string{10: "ten", 2: "two", 33: "thirty-three"},
			Floats: []float32{1.1, 16777216.0, 3.4028235e38, 1e-45}, Ok: true}
		compact, err := json.Marshal(r)
		if err != nil {
			panic(err)
		}
		indented, err := json.MarshalIndent(r, "", "  ")
		if err != nil {
			panic(err)
		}
		fmt.Fprintf(w, "inline constexpr std::string_view go_record_compact = %s;\n", quote(compact))
		fmt.Fprintf(w, "inline constexpr std::string_view go_record_indented = %s;\n", quote(indented))
	}

	// --- JSONTestSuite ---
	home, _ := os.UserHomeDir()
	dir := filepath.Join(home, "Programming", "oracles", "JSONTestSuite", "test_parsing")
	entries, err := os.ReadDir(dir)
	if err != nil {
		panic(err)
	}
	var names []string
	for _, e := range entries {
		if strings.HasSuffix(e.Name(), ".json") {
			names = append(names, e.Name())
		}
	}
	sort.Strings(names)
	fmt.Fprintln(w, "struct suite_file { std::string_view name; bool valid; bool parses; uint64_t tokens; uint64_t compact; uint64_t pretty; };")
	fmt.Fprintln(w, "inline constexpr suite_file suite[] = {")
	for _, n := range names {
		data, err := os.ReadFile(filepath.Join(dir, n))
		if err != nil {
			panic(err)
		}
		th, valid := tokens(data)
		c, parses := canonical(data)
		var ch, ph uint64
		if parses {
			f := newFnv()
			f.addString(c)
			ch = f.h
			var buf bytes.Buffer
			if err := json.Indent(&buf, []byte(c), "", "  "); err != nil {
				panic(err)
			}
			f = newFnv()
			f.add(buf.Bytes())
			ph = f.h
		}
		fmt.Fprintf(w, "    {%s, %v, %v, 0x%016xull, 0x%016xull, 0x%016xull},\n", quote([]byte(n)), valid, parses, th, ch, ph)
	}
	fmt.Fprintln(w, "};")
	fmt.Fprintln(w, "}")
}
