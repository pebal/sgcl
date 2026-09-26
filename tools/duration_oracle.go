// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for sgcl/core/duration.h: Go's time.ParseDuration,
// Duration.String, Seconds, Minutes, Hours, Microseconds, Milliseconds,
// Abs, Truncate and Round asked the same questions, the answers written
// out as a C++ header the test includes. Run it from the root of the
// tree:
//
//	go run tools/duration_oracle.go > tests/core/duration_cases.h
//
// Every input is listed below by hand; to them the program adds inputs
// drawn from a fixed seed (one to four numbers of up to twelve digits
// with a fraction of up to twenty or none, each with a unit, a sign or
// none, and one in three of them spoiled by a token inserted, dropped or
// replaced), and a set of values from the same seed over every magnitude
// from a nanosecond to the ends of the range to the ones asked of String
// and the arithmetic.
//
// Where the two differ the case is left out of Go's table rather than
// bent until it passes:
//
//   - a fraction. Go multiplies the fraction's digits by the unit in
//     float64 and truncates what comes out; sgcl takes the fraction
//     exactly and truncates that. Every parse with a fraction is checked
//     here against math/big as well, and the ones where Go's float64
//     lands off the exact value go to a table of their own, ExactParse,
//     whose answers are math/big's, not Go's.
//   - the error's text. Go's messages quote the input; sgcl's say what
//     was expected and give the byte offset. Only the refusal is compared.
//   - wrapping. Go's + - * wrap around silently; sgcl saturates. Only the
//     functions that saturate in Go too (Abs, Round) are asked here.
package main

import (
	"fmt"
	"math/big"
	"math/rand"
	"regexp"
	"strconv"
	"strings"
	"time"
)

var parseInputs = []string{
	// zero, signs
	"0", "+0", "-0", "0s", "-0s", "+0s", "0.0s", "0h0m0s",
	// one unit each
	"10ns", "11us", "12µs", "12μs", "13ms", "14s", "15m", "16h",
	"5s", "30s", "1478s", "-5s", "+5s",
	// fractions
	"5.0s", "5.6s", "5.s", ".5s", "1.0s", "1.00s", "1.004s", "1.0040s",
	"100.00100s", "-.5s", "+.5s", "0.5ns", "1.5ns", "1.999ns", ".000000001s",
	"0.0000000009s", "1.5h", "-1.5h", "0.3h", "0.1m", "2.5ms", "1.25us",
	"0.3333333333333333333h", "0.100000000000000000000h", "0.830103483285477580700h",
	"1.00000000000000000001s", "0.9999999999999999999999999999ns",
	"0.1234567890123456789012345678901234567890s", "7.77777777777777777777h",
	"0.000000000000000000000000000000000001h", "123.456789123456789m",
	"0.999999999999999999999999s", "2562047.788015215h",
	// several units, any order, repeated
	"3h30m", "10.5s4m", "-2m3.4s", "1h2m3s4ms5us6ns", "39h9m14.425s",
	"1h1h", "1m1h", "1ns1us1ms1s1m1h", "1h0m0s", "0h0m1s", "1.5h1.5m1.5s",
	// big values, the ends of the range
	"52763797000ns", "9007199254740993ns", "9223372036854775807ns",
	"9223372036854775.807us", "9223372036854s775ms807ns",
	"-9223372036854775808ns", "-9223372036854775.808us",
	"-9223372036854s775ms808ns", "-2562047h47m16.854775808s",
	"2562047h47m16.854775807s", "000000000000000000000001s",
	"0000000000000000000000000000000000000000000000000000000000001ns",
	"2562047h", "-2562047h", "153722867m", "106751d",
	// refusals
	"", "3", "-", "+", "s", ".", "-.", ".s", "+.s", "1d", "1 s", " 1s",
	"1s ", "\x85\x85", "\xffff", "hello \xffff world",
	"9223372036854775808ns", "9223372036854775.808us",
	"9223372036854ms775us808ns", "-9223372036854775809ns",
	"2562047h47m16.854775808s", "-2562047h47m16.854775809s", "0.0", "00",
	"1.5.5s", "1e3s", "1ss", "--1s", "-+1s", "+-1s", "1h-1m", "1µ", "1S",
	"1H", "1Ms", "18446744073709551616s", "99999999999999999999999ns",
	"1.s.", "1s1", "1s.", "3000000h", "2562048h", "1\x00s", "µs", "1μ",
	"1\xb5s", "1\xc2s", "1s\xc2", "0.", "-0.", "+", "1s-", "1s+1s",
	"1h30", "1..5s",
}

// Values Go writes as text and does the arithmetic on
var stringValues = []int64{
	0, 1, 2, 9, 10, 99, 100, 999, 1000, 1001, 1010, 1100, 1500, 1999,
	9999, 10000, 99999, 999999, 1000000, 1000001, 1234567, 1500000,
	999999999, 1000000000, 1000000001, 1500000000, 59000000000,
	59999999999, 60000000000, 61000000000, 90000000000, 3599999999999,
	3600000000000, 3601000000000, 3660000000000, 3661000000001,
	86400000000000, 36000000000000, 1<<53 + 1,
	-1, -999, -1000, -1500, -1000000000, -90000000000, -3600000000000,
	9223372036854775807, -9223372036854775808, -9223372036854775807,
	9223372036854775806, -9223372036854775806,
}

// Steps for Truncate and Round: the usual units, odd ones, a step of
// zero and a negative one (which leave the duration as it is), the
// largest step
var steps = []int64{
	1, 2, 3, 7, 10, 1000, 1000000, 1000000000, 60000000000, 3600000000000,
	3600000000001, 0, -1, -1000000000, 9223372036854775807,
	1 << 62, 1<<62 + 1,
}

func cppQuote(s string) string {
	var b strings.Builder
	b.WriteByte('"')
	for i := 0; i < len(s); i++ {
		c := s[i]
		switch {
		case c == '"' || c == '\\':
			b.WriteByte('\\')
			b.WriteByte(c)
		case c < 0x20 || c >= 0x7f:
			fmt.Fprintf(&b, "\\x%02X\"\"", c) // closed and reopened, so that a hex digit after it is not read into the escape
		default:
			b.WriteByte(c)
		}
	}
	b.WriteByte('"')
	return b.String()
}

func int64Literal(v int64) string {
	if v == -9223372036854775808 {
		return "INT64_MIN"
	}
	return strconv.FormatInt(v, 10) + "ll"
}

func hexFloat(f float64) string {
	return strconv.FormatFloat(f, 'x', -1, 64)
}

var segment = regexp.MustCompile(`([0-9]*)(?:\.([0-9]*))?(ns|us|µs|μs|ms|s|m|h)`)

var units = map[string]int64{
	"ns": 1, "us": 1000, "µs": 1000, "μs": 1000, "ms": 1000000,
	"s": 1000000000, "m": 60000000000, "h": 3600000000000,
}

// The exact value of an accepted input with math/big: each number times
// its unit, truncated to the nanosecond, summed, the sign applied
func exact(s string) *big.Int {
	neg := false
	if s != "" && (s[0] == '-' || s[0] == '+') {
		neg = s[0] == '-'
		s = s[1:]
	}
	sum := new(big.Int)
	for _, m := range segment.FindAllStringSubmatch(s, -1) {
		unit := big.NewInt(units[m[3]])
		num := new(big.Int)
		den := big.NewInt(1)
		digits := m[1] + m[2]
		if digits != "" {
			num.SetString(digits, 10)
		}
		for range m[2] {
			den.Mul(den, big.NewInt(10))
		}
		num.Mul(num, unit)
		num.Quo(num, den)
		sum.Add(sum, num)
	}
	if neg {
		sum.Neg(sum)
	}
	return sum
}

var tokens = []string{"0", "1", "5", "9", ".", "h", "m", "s", "ms", "us", "µs", "μs", "ns", "-", "+", "x", " "}

func randomDigits(rng *rand.Rand, n int) string {
	var b strings.Builder
	for i := 0; i < n; i++ {
		b.WriteByte(byte('0' + rng.Intn(10)))
	}
	return b.String()
}

func randomInput(rng *rand.Rand) string {
	var parts []string
	switch rng.Intn(3) {
	case 0:
		parts = append(parts, "-")
	case 1:
		parts = append(parts, "+")
	}
	units := []string{"ns", "us", "µs", "μs", "ms", "s", "m", "h"}
	for k := rng.Intn(4) + 1; k > 0; k-- {
		parts = append(parts, randomDigits(rng, rng.Intn(13)))
		if rng.Intn(2) == 0 {
			parts = append(parts, ".", randomDigits(rng, rng.Intn(21)))
		}
		parts = append(parts, units[rng.Intn(len(units))])
	}
	if rng.Intn(3) == 0 {
		i := rng.Intn(len(parts))
		switch rng.Intn(3) {
		case 0:
			parts = append(parts[:i], append([]string{tokens[rng.Intn(len(tokens))]}, parts[i:]...)...)
		case 1:
			parts = append(parts[:i], parts[i+1:]...)
		default:
			parts[i] = tokens[rng.Intn(len(tokens))]
		}
	}
	return strings.Join(parts, "")
}

func main() {
	rng := rand.New(rand.NewSource(20260924))
	inputs := append([]string{}, parseInputs...)
	for i := 0; i < 3000; i++ {
		inputs = append(inputs, randomInput(rng))
	}
	values := append([]int64{}, stringValues...)
	for bits := 1; bits <= 63; bits++ {
		for k := 0; k < 3; k++ {
			v := rng.Int63n(int64(1) << (bits - 1))
			v |= int64(1) << (bits - 1)
			if rng.Intn(2) == 0 {
				v = -v
			}
			values = append(values, v)
		}
	}

	type parseCase struct {
		text string
		ok   bool
		ns   int64
	}
	var goParse, exactParse []parseCase
	for _, s := range inputs {
		d, err := time.ParseDuration(s)
		if err != nil {
			goParse = append(goParse, parseCase{s, false, 0})
			continue
		}
		e := exact(s)
		if e.IsInt64() && e.Int64() == int64(d) {
			goParse = append(goParse, parseCase{s, true, int64(d)})
		} else {
			exactParse = append(exactParse, parseCase{s, true, e.Int64()})
		}
	}

	fmt.Println("//------------------------------------------------------------------------------")
	fmt.Println("// SGCL: a C++20 application framework")
	fmt.Println("// Copyright (c) 2022-2026 Sebastian Nibisz")
	fmt.Println("// SPDX-License-Identifier: Apache-2.0")
	fmt.Println("//------------------------------------------------------------------------------")
	fmt.Println("#pragma once")
	fmt.Println()
	fmt.Println("// Generated by tools/duration_oracle.go: do not edit.")
	fmt.Println("//")
	fmt.Println("// What Go's time package answers for the inputs and the values listed in")
	fmt.Println("// that program, which also says which cases are deliberately not here and")
	fmt.Println("// why. The floating results are hexadecimal literals: exact.")
	fmt.Println()
	fmt.Println("#include <cstdint>")
	fmt.Println()
	fmt.Println("namespace oracle {")
	fmt.Println("    struct DurationParseCase {")
	fmt.Println("        const char* text;")
	fmt.Println("        bool ok;")
	fmt.Println("        int64_t ns;")
	fmt.Println("    };")
	fmt.Println()
	fmt.Println("    struct DurationValueCase {")
	fmt.Println("        int64_t ns;")
	fmt.Println("        const char* text;")
	fmt.Println("        double seconds;")
	fmt.Println("        double minutes;")
	fmt.Println("        double hours;")
	fmt.Println("        int64_t microseconds;")
	fmt.Println("        int64_t milliseconds;")
	fmt.Println("        int64_t abs;")
	fmt.Println("    };")
	fmt.Println()
	fmt.Println("    struct DurationStepCase {")
	fmt.Println("        int64_t ns;")
	fmt.Println("        int64_t step;")
	fmt.Println("        int64_t truncated;")
	fmt.Println("        int64_t rounded;")
	fmt.Println("    };")
	fmt.Println()
	fmt.Printf("    // %s\n", "time.ParseDuration")
	fmt.Println("    inline constexpr DurationParseCase GoParseDuration[] = {")
	for _, c := range goParse {
		fmt.Printf("        {%s, %v, %s},\n", cppQuote(c.text), c.ok, int64Literal(c.ns))
	}
	fmt.Println("    };")
	fmt.Println()
	fmt.Println("    // Inputs Go accepts with a value its float64 puts off the exact one: the")
	fmt.Println("    // value math/big gives, each number times its unit truncated to the")
	fmt.Println("    // nanosecond")
	fmt.Println("    inline constexpr DurationParseCase ExactParse[] = {")
	if len(exactParse) == 0 {
		fmt.Println("        {\"0\", true, 0},")
	}
	for _, c := range exactParse {
		fmt.Printf("        {%s, %v, %s},\n", cppQuote(c.text), c.ok, int64Literal(c.ns))
	}
	fmt.Println("    };")
	fmt.Println()
	fmt.Println("    // Duration.String, Seconds, Minutes, Hours, Microseconds, Milliseconds, Abs")
	fmt.Println("    inline constexpr DurationValueCase GoDurationValues[] = {")
	for _, v := range values {
		d := time.Duration(v)
		fmt.Printf("        {%s, %s, %s, %s, %s, %s, %s, %s},\n", int64Literal(v), cppQuote(d.String()),
			hexFloat(d.Seconds()), hexFloat(d.Minutes()), hexFloat(d.Hours()),
			int64Literal(d.Microseconds()), int64Literal(d.Milliseconds()), int64Literal(int64(d.Abs())))
	}
	fmt.Println("    };")
	fmt.Println()
	fmt.Println("    // Duration.Truncate and Duration.Round")
	fmt.Println("    inline constexpr DurationStepCase GoDurationSteps[] = {")
	for _, v := range values {
		for _, m := range steps {
			d := time.Duration(v)
			fmt.Printf("        {%s, %s, %s, %s},\n", int64Literal(v), int64Literal(m),
				int64Literal(int64(d.Truncate(time.Duration(m)))), int64Literal(int64(d.Round(time.Duration(m)))))
		}
	}
	fmt.Println("    };")
	fmt.Println("}")
}
