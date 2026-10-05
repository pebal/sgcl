// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle of tests/net/http/cookie_jar_go.cpp: Go's net/http/cookiejar
// (with a nil PublicSuffixList: x/net/publicsuffix is not in the standard
// library) fed the same script as the module's jar. One operation a line on
// stdin, tab-separated:
//
//	set URL SET-COOKIE   the Set-Cookie value as a response from URL carries it
//	get URL              prints the Cookie field a request to URL carries
//
// The Set-Cookie value is read by net/http as a response's header, the
// Cookie field written as Request.AddCookie writes it.
package main

import (
	"bufio"
	"fmt"
	"net/http"
	"net/http/cookiejar"
	"net/url"
	"os"
	"strings"
)

func main() {
	jar, _ := cookiejar.New(nil)
	in := bufio.NewScanner(os.Stdin)
	in.Buffer(make([]byte, 1<<20), 1<<20)
	out := bufio.NewWriter(os.Stdout)
	defer out.Flush()
	for in.Scan() {
		parts := strings.SplitN(in.Text(), "\t", 3)
		u, err := url.Parse(parts[1])
		if err != nil {
			fmt.Fprintln(out, "bad url")
			continue
		}
		switch parts[0] {
		case "set":
			res := &http.Response{Header: http.Header{"Set-Cookie": {parts[2]}}}
			jar.SetCookies(u, res.Cookies())
		case "get":
			req, _ := http.NewRequest("GET", parts[1], nil)
			for _, c := range jar.Cookies(u) {
				req.AddCookie(c)
			}
			fmt.Fprintln(out, req.Header.Get("Cookie"))
		}
	}
}
