// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"bytes"
	"context"
	"strings"
	"testing"

	"fuchsia.googlesource.com/tools/botanist"
	"fuchsia.googlesource.com/tools/testsharder"
)

func TestTester(t *testing.T) {
	cases := []struct {
		name   string
		test   testsharder.Test
		stdout string
		stderr string
	}{
		{
			name: "should run a command a local subprocess",
			test: testsharder.Test{
				Name: "hello_world_test",
				// Assumes that we're running on a Unix system.
				Command: []string{"/bin/echo", "Hello world!"},
			},
			stdout: "Hello world!",
		},
	}

	for _, tt := range cases {
		t.Run(tt.name, func(t *testing.T) {
			tester := &SubprocessTester{}

			stdout := new(bytes.Buffer)
			stderr := new(bytes.Buffer)

			if err := tester.Test(context.Background(), tt.test, stdout, stderr); err != nil {
				t.Errorf("failed to exected test %q: %v", tt.test.Name, err)
				return
			}

			// Compare stdout
			actual := strings.TrimSpace(stdout.String())
			expected := tt.stdout
			if actual != expected {
				t.Errorf("got stdout %q but wanted %q", actual, expected)
			}

			// Compare stderr
			actual = strings.TrimSpace(stderr.String())
			expected = tt.stderr
			if actual != expected {
				t.Errorf("got stderr %q but wanted %q", actual, expected)
			}
		})
	}
}

// Verifies that SSHTester can execute tests on a remote device. These tests are
// only meant for local verification.  You can execute them like this:
//
//  DEVICE_CONTEXT=./device.json go test ./...
func TestSSHTester(t *testing.T) {
	t.Skip("ssh tests are meant for local testing only")

	devCtx, err := botanist.GetDeviceContext()
	if err != nil {
		t.Fatal(err)
	}

	cases := []struct {
		name   string
		tests  []testsharder.Test
		stdout string
		stderr string
	}{
		{
			name: "should run a command over SSH",
			tests: []testsharder.Test{
				{
					Name: "hello_world_test",
					// Just 'echo' and not '/bin/echo' because this assumes we're running on
					// Fuchsia.
					Command: []string{"echo", "Hello world!"},
				},
			},
			stdout: "Hello world!",
		},
		{
			name: "should run successive commands over SSH",
			tests: []testsharder.Test{
				{
					Name:    "test_1",
					Command: []string{"echo", "this is test 1"},
				},
				{
					Name:    "test_2",
					Command: []string{"echo", "this is test 2"},
				},
			},
			stdout: "this is test 1\nthis is test 2",
		},
	}

	for _, tt := range cases {
		t.Run(tt.name, func(t *testing.T) {
			tester, err := NewSSHTester(*devCtx)
			if err != nil {
				t.Errorf("failed to intialize tester: %v", err)
				return
			}

			stdout := new(bytes.Buffer)
			stderr := new(bytes.Buffer)
			for _, test := range tt.tests {
				if err := tester.Test(context.Background(), test, stdout, stderr); err != nil {
					t.Error(err)
					return
				}
			}

			if err := tester.Close(); err != nil {
				t.Fatalf("failed to close tester: %v", err)
			}

			// Compare stdout
			actual := strings.TrimSpace(stdout.String())
			expected := tt.stdout
			if actual != expected {
				t.Errorf("got stdout %q but wanted %q", actual, expected)
			}

			// Compare stderr
			actual = strings.TrimSpace(stderr.String())
			expected = tt.stderr
			if actual != expected {
				t.Errorf("got stderr %q but wanted %q", actual, expected)
			}
		})
	}
}
