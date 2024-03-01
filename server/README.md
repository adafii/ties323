# Log of setting up web, email, and DNS servers

## Initial setup

- My own Linux desktop, which I use to set up everything remotely
- Two low-spec VPS servers with public IP4 and IP6 addresses
    - ns-ofu, dns server 
    - mail-ofu, mail and web server
- Domain name ofu.fi

## Operating system installation

### Installed OpenBSD 7.4 to mail-ofu (30 minutes)

- I started by installing OpenBSD to mail-ofu. My VPS provider offers an option to boot KVM console into wide selection of installation images, which made this part easy.
- OpenBSD installer is pretty straightforward and I have used it before so the installation was quick.
- Only issue I run into was that first I used GPT partitioning and realized after failed reboot that the hypervisor only supported MBR, so I had to reinstall with correct MBR partitioning.
- Finally, I made some minimal post install configurations:
    - Updated the system and packages (syspatch & pkg_add -Uu)
    - Installed my favourite text editor
    - Configured doas (similar to sudo) so that normal user can run commands as root
    - Configured public key SSH login for normal user, and disabled root and password login
    - Moved SSH to non-standard port 2288 to hopefully decrease random login attempts

### Installed AlmaLinux 9.3 to ns-ofu (50 minutes)

- I wanted to try this RHEL clone. Unfortunately, I couldn't install this manually through KVM console because both GUI and CLI installers didn't work with the console
- There is option to use installer through VNC, but VNC client refused to connect to the installer
- VPS provider had an option to use pre-installed disk image, which I installed to ns-ofu
- Post installation steps:
    - Updated the system (dnf update)
    - Created normal user and made sure it can run sudo commands as root
    - Configured public key SSH login for normal user, and disabled root and password login
    - Moved SSH to port 2288

## Network

### Network configuration (30 minutes)

- It seems that both servers got correct network configuration from dhcp.
- Conceptually, the network configuration looks like this
![network](images/network.png)
- VPS provider offered an option to connect servers to a private network, 10.0.0.0/24. This was quick to set up and might come useful later.
- I noticed that the servers are isolated in the private network and can only communicate through the router 10.0.0.1

### Setting firewal for ns-ofu (40 minutes)

- By default there is no firewall service running on AlmaLinux.
- I decided to use nftables to configure firewall. 
- Getting the firewall rules right took some studying, because I haven't used nftables much. Luckily, there was a simple example configuration and [RHEL manuals](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/9/html/configuring_firewalls_and_packet_filters/getting-started-with-nftables_firewall-packet-filters).
- I edited the ruleset to allow: 
    - Incoming TCP connections to SSH port 2288 
    - Established TCP connections
    - ICMP
- After enabling and starting nftables service:
```
$ sudo nft list ruleset
table inet nftables_svc {
	set allowed_protocols {
		type inet_proto
		elements = { icmp, ipv6-icmp }
	}

	set allowed_interfaces {
		type ifname
		elements = { "lo" }
	}

	set allowed_tcp_dports {
		type inet_service
		elements = { 2288 }
	}

	chain allow {
		ct state established,related accept
		meta l4proto @allowed_protocols accept
		iifname @allowed_interfaces accept
		tcp dport @allowed_tcp_dports accept
	}

	chain INPUT {
		type filter hook input priority 20; policy accept;
		jump allow
		reject
	}

	chain FORWARD {
		type filter hook forward priority 20; policy accept;
		jump allow
		reject with icmpx host-unreachable
	}
}
```


