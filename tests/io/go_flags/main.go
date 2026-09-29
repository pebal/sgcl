// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The Go side of tests/io/flags.cpp: Go's flag package given the flags the
// test gives io::flags and the same command lines, its answers printed for
// the test to compare byte for byte:
//
//	go_flags < cases
//
// The input is one case a line, its arguments separated by the byte 0x1f.
// The output is the usage (PrintDefaults) once, then for each case the
// error Parse returned, or "nil" with every flag's value after it,
// quoted, and the arguments left, quoted.
package main

import (
	"bufio"
	"bytes"
	"flag"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"
	"time"
)

func define() *flag.FlagSet {
	fs := flag.NewFlagSet("prog", flag.ContinueOnError)
	fs.SetOutput(io.Discard)
	fs.Int("port", 8080, "the `port` to listen on")
	fs.String("host", "localhost", "the host")
	fs.Bool("v", false, "verbose")
	fs.Bool("tls", true, "serve over TLS")
	fs.Duration("timeout", 5*time.Second, "how long to wait")
	fs.Float64("ratio", 0.5, "a ratio")
	fs.Int64("n", 0, "a count")
	fs.Uint64("size", 0, "a size")
	fs.String("name", "", "a name")
	fs.String("note", "a \"quoted\"\ttab", "a note\non two lines")
	fs.Float64("big", 1e6, "a large number")
	return fs
}

func main() {
	var usage bytes.Buffer
	fs := define()
	fs.SetOutput(&usage)
	fs.PrintDefaults()
	fmt.Print("=== usage\n", usage.String(), "=== end\n")
	in := bufio.NewScanner(os.Stdin)
	in.Buffer(make([]byte, 1<<20), 1<<20)
	for in.Scan() {
		line := in.Text()
		var args []string
		if line != "" {
			args = strings.Split(line, "\x1f")
		}
		fs := define()
		err := fs.Parse(args)
		fmt.Println("=== case")
		if err != nil {
			// what a refused value leaves in its variable is Go's own
			// (Set stores ParseInt's 0 or its limit before it fails;
			// io::flags leaves the variable as it was), so only the error
			fmt.Println("err " + err.Error())
			continue
		}
		fmt.Println("err nil")
		fs.VisitAll(func(f *flag.Flag) {
			fmt.Println(f.Name + "=" + strconv.Quote(f.Value.String()))
		})
		for _, a := range fs.Args() {
			fmt.Println("arg " + strconv.Quote(a))
		}
	}
}
