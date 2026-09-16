//go:build !arm64

package main

// no pause instruction here: the spin is the loop alone
func isb() {}
