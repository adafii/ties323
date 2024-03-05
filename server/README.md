# Notes of setting up web, email, and DNS servers

The goal of this project is to set up web, email, and DNS services on real servers. To make the project more interesting, I plan to open at least the web and DNS services to the public internet. Although I am not an IT security expert, I will do my best to protect the servers from unauthorized access. There isn't any valuable information on the servers, but in the worst case, the servers and the services on them could be used to launch further attacks and spamming. 

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

### Setting firewall for ns-ofu (40 minutes)

- I decided to use nftables to configure firewall. 
- Getting the firewall rules right took some studying, because I haven't used nftables much. Luckily, there was a simple example configuration and [RHEL manuals](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/9/html/configuring_firewalls_and_packet_filters/getting-started-with-nftables_firewall-packet-filters).
- I edited the ruleset to allow: 
    - Incoming TCP connections to SSH port 2288
    - Returning traffic
    - ICMP
- Outgoing traffic seems to be allowed by default
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

    set blocked_interfaces {
        type ifname
        elements = { "eth1" }
    }

    set allowed_tcp_dports {
        type inet_service
        elements = { 2288 }
    }

    chain allow {
        ct state established,related accept
        meta l4proto @allowed_protocols accept
        iifname @allowed_interfaces accept
        iifname @blocked_interfaces drop
        tcp dport @allowed_tcp_dports accept
    }

    chain INPUT {
        type filter hook input priority 20; policy accept;
        jump allow
        drop
    }

    chain FORWARD {
        type filter hook forward priority 20; policy accept;
        jump allow
        drop
    }
}
```

### Setting firewall for mail-ofu (50 minutes)

- OpenBSD uses PF as firewall. It's enabled by default, but the ruleset should be still customized to be strict as possible.
- [PF user's guide](https://www.openbsd.org/faq/pf/index.html) was a good resource for setting up the firewall 
- I configured PF to allow 
    - Outgoing TCP, UDP and ICMP connections (I might restrict this more later if needed)
    - Incoming TCP connections to SSH port 2288 
- PF tracks the state by default, so returning traffic, both ways, always passes unless explicitly denied
- After setting the rules:
```
mail$ doas pfctl -sr
match in all scrub (no-df)
block drop all
pass out on egress proto tcp all flags S/SA modulate state
pass out on egress proto udp all
pass out on egress proto icmp all
pass in on egress proto tcp from any to any port = 2288 flags S/SA
```

## Configuring DNS

### Configuring ns-ofu as a primary DNS (3 hours)

- Again, I used [RHEL manuals](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/9/html/managing_networking_infrastructure_services/assembly_setting-up-and-configuring-a-bind-dns-server_networking-infrastructure-services) as my primary source.
- To find more detailed information, I used [BIND manual](https://bind9.readthedocs.io/en/latest/index.html).
- I installed bind to ns-ofu (dnf install bind).
- SELinux was set from permissive mode to enforcing mode to harden bind against known vulnerabilities.
- Edited /etc/hosts to 
```
65.108.60.126           ns.ofu.fi ns-ofu
127.0.0.1               localhost

2a01:4f9:c012:7e00::1   ns.ofu.fi ns-ofu
::1                     localhost
```
- By default, bind is configured to act as a local DNS resolver server. I want it to act as a public authoritative server.
- I changed /etc/named.conf to disable recursion and accept connections from the public internet.
- /etc/named.conf also needs to have forward zone definition:
```
zone "ofu.fi" {
        type master;
        file "ofu.fi.zone";
        allow-query { any; };
        allow-transfer { none; };
};
```
- Config file was validated with named-checkconf command.
- File /var/named/ofu.fi.zone was added:
```
$TTL 8h
@	        IN          SOA         ns.ofu.fi. admin.oru.fi. (
                                    2024030401 ; serial number
                                    1d         ; refresh period
                                    3h         ; retry period
                                    3d         ; expire time
                                    3h )       ; minimum TTL

            IN          NS          ns.ofu.fi.
ns          IN          A           65.108.60.126
ns          IN          AAAA        2a01:4f9:c012:7e00::1
```
- Mail address is not a typo, it's my other domain.
- Changed file permissions according to RHEL manual and validated zone-file:
```
# chown root:named /var/named/ofu.fi.zone
# chmod 640 /var/named/ofu.fi.zone
# named-checkzone ofu.fi /var/named/ofu.fi.zone
zone ofu.fi/IN: loaded serial 2024030401
OK
```
- I opened port 53 for udp and tcp traffic from nftables config and reloaded nftables
- Finally, 'systemctl enable --now named' to enable and start bind
- I can now query records from my own desktop
```
% dig +nocomment @65.108.60.126 ofu.fi any

; <<>> DiG 9.18.24 <<>> +nocomment @65.108.60.126 ofu.fi any
; (1 server found)
;; global options: +cmd
;ofu.fi.				IN	ANY
ofu.fi.			28800	IN	SOA	ns.ofu.fi. admin.oru.fi. 2024030401 86400 10800 259200 10800
ofu.fi.			28800	IN	NS	ns.ofu.fi.
ns.ofu.fi.		28800	IN	A	65.108.60.126
ns.ofu.fi.		28800	IN	AAAA	2a01:4f9:c012:7e00::1
;; Query time: 6 msec
;; SERVER: 65.108.60.126#53(65.108.60.126) (TCP)
;; WHEN: Tue Mar 05 21:21:48 EET 2024
;; MSG SIZE  rcvd: 172
```
- At this point I went to my domain registrar site to set glue records and nameservers. This made me realize that I probably need a secondary nameserver to make this actually work.
- I am not giving up yet. DNS might work if I use mail-ofu as my secondary DNS.

### Configuring mail-ofu as a secondary DNS 

- I used RHEL manual and [NSD docs](https://nsd.docs.nlnetlabs.nl/en/latest/index.html) as my source for this section.
- OpenBSD manual entry for [nsd.conf](https://man.openbsd.org/nsd.conf.5) was also helpful for OpenBSD specific configs.
- Zone file on ns-ofu was changed to:
```
cat /var/named/ofu.fi.zone
$TTL 8h
@           IN          SOA         ns.ofu.fi. admin.oru.fi. (
                                    2024030401 ; serial number
                                    1d         ; refresh period
                                    3h         ; retry period
                                    3d         ; expire time
                                    3h )       ; minimum TTL

@           IN          NS          ns.ofu.fi.
@           IN          NS          ns2.ofu.fi.

ns          IN          A           65.108.60.126
ns          IN          AAAA        2a01:4f9:c012:7e00::1

ns2         IN          A           95.217.16.28
```
- Forward zone definition on ns-ofu named.conf was changed to:
```
key "ofu-transfer-key" {
	algorithm hmac-sha256;
	secret <redacted>;
};

zone "ofu.fi" {
	type master;
	file "ofu.fi.zone";
	allow-query { any; };
	allow-transfer { key ofu-transfer-key; };
};
```
- Changes above were validated and reloaded.
- I created /var/nsd/etc/nsd.conf to mail-ofu:
```
mail# cat /var/nsd/etc/nsd.conf
server:
    server-count: 1
    database: ""
    zonelistfile: "/var/nsd/db/zone.list"
    username: _nsd
    logfile: "/var/log/nsd.log"
    pidfile: ""
    xfrdfile: "/var/nsd/run/xfrd.state"

remote-control:
    control-enable: yes
    control-interface: /var/run/nsd.sock

key:
    name: ofu-transfer-key
    algorithm: hmac-sha256
    secret: <redacted>

zone:
    name: "ofu.fi"
    zonefile: "slave/ofu.fi.signed"
    allow-notify: 65.108.60.126 NOKEY
    request-xfr: 65.108.60.126 ofu-transfer-key
```
- Pf rules was changed to accept tcp and udp connections to port 53.
- Enabled and started nsd:
```
mail# rcctl enable nsd
mail# rcctl start nsd
```
- Nsd started
```

```
