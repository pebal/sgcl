// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// Same shapes as benchmarks/txt/html_stencil.cpp: a template read once and
// written many times with Go's html/template, over a stream of different
// values with characters to escape.
//   html_stencil [op=link] [count]
// ops: link (a link, its title and text), rows (a table of ten rows, an
// attribute, a URL and text in each), script (an object and a string in a
// script), parse (the rows template read). Prints nanoseconds per operation.
package main

import (
	"fmt"
	"html/template"
	"os"
	"strconv"
	"strings"
	"time"
)

const values = 1024

func sourceFor(op string) string {
	switch op {
	case "link":
		return `<a href="/u/{{ .id }}" title="{{ .name }}">{{ .name }}</a>`
	case "script":
		return `<script>var cfg = {{ .cfg }}; var s = "{{ .name }}";</script>`
	default:
		return `<table>{{ range .rows }}<tr><td title="{{ .t }}">{{ .t }}</td><td><a href="{{ .u }}">x</a></td></tr>{{ end }}</table>`
	}
}

func word(i int) string {
	return fmt.Sprintf("Ada & <Bob> \"%d\" o'neil", i)
}

func main() {
	op := "link"
	if len(os.Args) > 1 {
		op = os.Args[1]
	}
	count := 400000
	if len(os.Args) > 2 {
		count, _ = strconv.Atoi(os.Args[2])
	}
	data := make([]map[string]any, values)
	for i := range data {
		rows := make([]any, 10)
		for r := range rows {
			rows[r] = map[string]any{"t": word(i + r), "u": "/p?q=" + word(i+r)}
		}
		data[i] = map[string]any{"id": i, "name": word(i), "cfg": map[string]any{"id": i, "name": word(i)}, "rows": rows}
	}
	sink := 0
	if op == "parse" {
		start := time.Now()
		for i := 0; i < count; i++ {
			t := template.Must(template.New("x").Parse(sourceFor("rows")))
			var b strings.Builder
			t.Execute(&b, data[i%values]) // html/template escapes at the first execution
			sink += b.Len()
		}
		fmt.Printf("go op=%s ns/op=%.1f (%d)\n", op, float64(time.Since(start).Nanoseconds())/float64(count), sink%7)
		return
	}
	t := template.Must(template.New("x").Parse(sourceFor(op)))
	var warm strings.Builder
	t.Execute(&warm, data[0])
	start := time.Now()
	for i := 0; i < count; i++ {
		var b strings.Builder
		t.Execute(&b, data[i%values])
		sink += b.Len()
	}
	fmt.Printf("go op=%s ns/op=%.1f (%d)\n", op, float64(time.Since(start).Nanoseconds())/float64(count), sink%7)
}
