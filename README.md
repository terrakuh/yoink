# Yoink them into oblivion

A small and modern tool to parse logs and ban clients. Just like fail2ban.

## NFT Config

```nft
table inet yoinkd {
	set blacklist4 {
		type ipv4_addr
	}

	chain input {
		type filter hook input priority filter + 1
		policy accept

		ip saddr @blacklist4 drop
	}
}
```

## Security

Blindly running a program as root is generally a bad idea. This is no exception. Special care must be taken because this tool matches the variables given in the config from a log file and passes them to another defined process. For this reason:

- **Run this program as an unprivileged user**
- **When root privileges are required (e.g. modifying the firewall) constrain it with something like setuid bit or `sudo` with the command constraints**
- **Make the variables as strict as possible**
