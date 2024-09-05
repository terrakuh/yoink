# Yoink them into oblivion

A small and modern tool to parse logs and ban clients. Just like fail2ban.

## Security

Blindly running a program as root is generally a bad idea. This is no exception. Special care must be taken because this tool matches the variables given in the config from a log file and passes them to another defined process. For this reason:

- **Run this program as an unprivileged user**
- **When root privileges are required (e.g. modifying the firewall) constrain it with something like setuid bit or `sudo` with the command constraints**
- **Make the variables as strict as possible**
