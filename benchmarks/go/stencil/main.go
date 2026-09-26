// Same shape as benchmarks/txt/stencil.cpp: a template read once and
// written many times, over a stream of a thousand different values.
//   stencil [op=simple] [count]
// ops: empty simple five spec branch pipe deep miss rows rows100 to
// `to` writes into a buffer the caller keeps (bytes.Buffer, Reset each
// turn); every other op builds a fresh string, which is what render()
// does on the other side.
// Prints nanoseconds per operation.
package main

import (
	"fmt"
	"math/rand"
	"os"
	"strconv"
	"strings"
	"text/template"
	"time"
)

const values = 1024

func sourceFor(op string) string {
	switch op {
	case "empty":
		return "nothing happens here and it takes forty-five ch"
	case "simple":
		return "{{ .n }} left"
	case "five":
		return "{{ .a }} {{ .b }} {{ .c }} {{ .d }} {{ .e }}"
	case "spec":
		return `{{ printf "%8.2f" .d }}`
	case "branch":
		return "{{ if .on }}yes{{ else }}no{{ end }}"
	case "pipe":
		return "{{ .name | upper | trim }}"
	case "deep":
		return "{{ .a.b.c.d }}"
	case "miss":
		return "{{ .nosuchname }}"
	}
	return "<ul>{{ range .rows }}<li>{{ .who }}: " +
		"{{ if .on }}on{{ else }}off{{ end }}</li>{{ end }}</ul>"
}

func countFor(op string, given int) int {
	if given != 0 {
		return given
	}
	switch op {
	case "rows100":
		return 20000
	case "rows", "to":
		return 200000
	case "pipe":
		return 500000
	}
	return 1000000
}

func stream(op string, rows int) []any {
	rng := rand.New(rand.NewSource(1))
	out := make([]any, values)
	for i := range out {
		n := rng.Intn(1000000)
		d := float64(rng.Intn(100000)) + 0.5
		switch op {
		case "rows", "rows100", "to":
			made := make([]any, rows)
			for r := 0; r < rows; r++ {
				made[r] = map[string]any{
					"who": fmt.Sprintf("user%d", n+r),
					"on":  (n+r)&1 != 0,
				}
			}
			out[i] = map[string]any{"rows": made}
		case "deep":
			out[i] = map[string]any{"a": map[string]any{"b": map[string]any{
				"c": map[string]any{"d": n}}}}
		case "pipe":
			out[i] = map[string]any{"name": fmt.Sprintf("  user%d  ", n)}
		case "five":
			out[i] = map[string]any{"a": n, "b": n + 1, "c": n + 2, "d": n + 3, "e": n + 4}
		case "branch":
			out[i] = map[string]any{"on": n&1 != 0}
		default:
			out[i] = map[string]any{"n": n, "d": d, "on": n&1 != 0}
		}
	}
	return out
}

var sink int

func main() {
	// `parse` reads the same source over and over and writes nothing:
	// what the compiled form saves every time a page is written
	args := os.Args[1:]
	parseOnly := len(args) > 0 && args[0] == "parse"
	if parseOnly {
		args = args[1:]
	}
	op := "simple"
	if len(args) > 0 {
		op = args[0]
	}
	given := 0
	if len(args) > 1 {
		given, _ = strconv.Atoi(args[1])
	}
	count := countFor(op, given)
	rows := 10
	if op == "rows100" {
		rows = 100
	}
	funcs := template.FuncMap{
		"upper": strings.ToUpper,
		"lower": strings.ToLower,
		"trim":  strings.TrimSpace,
	}
	t, err := template.New("b").Funcs(funcs).Parse(sourceFor(op))
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	data := stream(op, rows)

	// a warm-up pass, as the other side does
	for i := 0; i < 1000; i++ {
		var b strings.Builder
		t.Execute(&b, data[i&(values-1)])
		sink += b.Len()
	}

	if parseOnly {
		source := sourceFor(op)
		start := time.Now()
		for i := 0; i < count; i++ {
			t, _ := template.New("b").Funcs(funcs).Parse(source)
			sink += len(t.Name())
		}
		fmt.Printf("go op=parse-%s count=%d ns/op=%.2f\n", op, count,
			float64(time.Since(start).Nanoseconds())/float64(count))
		return
	}

	start := time.Now()
	if op == "to" {
		var b strings.Builder
		for i := 0; i < count; i++ {
			b.Reset()
			t.Execute(&b, data[i&(values-1)])
			sink += b.Len()
		}
	} else {
		for i := 0; i < count; i++ {
			var b strings.Builder
			t.Execute(&b, data[i&(values-1)])
			sink += len(b.String())
		}
	}
	ns := float64(time.Since(start).Nanoseconds()) / float64(count)
	fmt.Printf("go op=%s count=%d ns/op=%.2f\n", op, count, ns)
}
