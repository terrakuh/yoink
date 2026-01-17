package main

import (
	"errors"
	"flag"
	"log/slog"
	"os"
	"os/exec"
	"strconv"
	"syscall"
	"time"
)

func main() {
	forceRoot := flag.Bool("forceroot", false, "disables user checks and permits running as root user")

	foreground := flag.Bool("foreground", false, "don't fork and run in the foreground")

	pidFile := flag.String("pidfile", "/var/run/yoinkd.pid", "the PID file for the background process")

	configPath := flag.String("config", "", "the config path for rules, etc.")

	flag.Parse()

	if info, err := os.Stat(*configPath); err != nil || info.IsDir() {
		slog.With("err", err).Error("Invalid config path")
		os.Exit(1)
	}

	if !*forceRoot && (os.Getuid() == 0 || os.Getgid() == 0) {
		slog.Error("Trying to run this as root. Consider https://github.com/terrakuh/yoink/README.md#Security")
		os.Exit(1)
	}

	if !*foreground {
		startBackground()
	}

	if *pidFile != "" {
		createPIDFile(*pidFile)
		defer os.Remove(*pidFile) //nolint:errcheck
	}

	<-time.After(5 * time.Second)
}

func startBackground() {
	args := append([]string{}, os.Args...)
	args[0] = "-foreground"
	cmd := exec.Command(os.Args[0], args...)
	if err := cmd.Start(); err != nil {
		slog.With("err", err).Error("Failed to launch program in background")
		os.Exit(1)
	}
	if err := cmd.Process.Release(); err != nil {
		slog.With("err", err).Warn("Failed to release starter resources")
	}
	os.Exit(0)
}

func createPIDFile(path string) {
	if data, err := os.ReadFile(path); err == nil {
		pid, err := strconv.ParseInt(string(data), 10, strconv.IntSize)
		if err != nil {
			slog.With("err", err, "file", path).Error("PID file already exists with an invalid PID")
			os.Exit(1)
		}

		var errno syscall.Errno
		err = syscall.Kill(int(pid), 0)
		if err == nil || (errors.As(err, &errno) && errno == syscall.ESRCH) {
			slog.With("pid", pid, "file", path).Error("yoink daemon is already running and using this PID file")
			os.Exit(1)
		}
	}

	err := os.WriteFile(path, []byte(strconv.FormatInt(int64(os.Getpid()), 10)), 0644)
	if err != nil {
		slog.With("err", err, "file", path).Error("Failed to write to PID file")
		os.Exit(1)
	}
}
