package main

import (
	"context"
	"io"
	"os/exec"
)

func monitor(ctx context.Context) {
	cmd := exec.CommandContext(ctx, "")
	r, w := io.Pipe()
	cmd.Stdout = w
	cmd.Start()

	for {
		r.Read()
	}

	cmd.Wait()
}
