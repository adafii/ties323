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
- Conceptually, the network configuration looks like this:
![network](images/network.png)
- VPS provider offered an option to connect servers to a private network, 10.0.0.0/24. This was quick to set up and might come useful later.
- I noticed that the servers are isolated in the private network and can only communicate through the router 10.0.0.1.

### Setting firewall for ns-ofu (40 minutes)

- I decided to use nftables to configure firewall. 
- Getting the firewall rules right took some studying, because I haven't used nftables much. Luckily, there was a simple example configuration and [RHEL manuals](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/9/html/configuring_firewalls_and_packet_filters/getting-started-with-nftables_firewall-packet-filters).
- I edited the ruleset to allow: 
    - Incoming TCP connections to SSH port 2288
    - Returning traffic
    - ICMP
- Outgoing traffic seems to be allowed by default.
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
- [PF user's guide](https://www.openbsd.org/faq/pf/index.html) was a good resource for setting up the firewall.
- I configured PF to allow:
    - Outgoing TCP, UDP and ICMP connections (I might restrict this more later if needed)
    - Incoming TCP connections to SSH port 2288 
- PF tracks the state by default, so returning traffic, both ways, always passes unless explicitly denied.
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
- Edited /etc/hosts to:
```
65.108.60.126           ns.ofu.fi ns-ofu
127.0.0.1               localhost

2a01:4f9:c012:7e00::1   ns.ofu.fi ns-ofu
::1                     localhost
```
- By default, bind is configured to act as a local DNS resolver. I want it to act as a public authoritative server.
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
- I opened port 53 for udp and tcp traffic from nftables config and reloaded nftables.
- Finally, 'systemctl enable --now named' to enable and start bind.
- I can now query records from my own workstation:
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
- Most of the time configuring ns-ofu DNS went to reading manuals so that I understand what I'm doing.
- At this point I went to my domain registrar site to set glue records and nameservers. This made me realize that I probably need a secondary nameserver to make this actually work.
- I am not giving up yet. DNS might work if I use mail-ofu as my secondary DNS.

### Configuring mail-ofu as a secondary DNS (3 hours)

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
- There was some trial and error to get the nsd.conf right, which took most of the time I used to setup nsd 
- Finally, everything seemed okay and 'nsd-checkconf /var/nsd/etc/nsd.conf' run without errors 
- Pf rules was changed to accept tcp and udp connections to port 53.
- Enabled and started nsd:
```
mail# rcctl enable nsd
mail# rcctl start nsd
```
- nsd daemon started:
```
mail# cat /var/log/nsd.log 
...
[2024-03-05 23:13:51.159] nsd[7636]: notice: nsd starting (NSD 4.7.0)
[2024-03-05 23:13:51.214] nsd[50235]: notice: nsd started (NSD 4.7.0), pid 48780
[2024-03-05 23:13:51.226] nsd[48780]: info: zone ofu.fi serial 0 is updated to 2024030401
```
- Serial is from ns-ofu zone file, which means that nsd managed to transfer records from primary dns.
- I can query mail-ofu DNS records from my own workstation: 
```
% dig +nocomment @95.217.16.28 ofu.fi NS

; <<>> DiG 9.18.24 <<>> +nocomment @95.217.16.28 ofu.fi NS
; (1 server found)
;; global options: +cmd
;ofu.fi.				IN	NS
ofu.fi.			28800	IN	NS	ns.ofu.fi.
ofu.fi.			28800	IN	NS	ns2.ofu.fi.
ns.ofu.fi.		28800	IN	A	65.108.60.126
ns2.ofu.fi.		28800	IN	A	95.217.16.28
ns.ofu.fi.		28800	IN	AAAA	2a01:4f9:c012:7e00::1
;; Query time: 6 msec
;; SERVER: 95.217.16.28#53(95.217.16.28) (UDP)
;; WHEN: Wed Mar 06 00:14:38 EET 2024
;; MSG SIZE  rcvd: 158
```
- It seems that at least forward zones should be set up right now.

### Setting authoritative DNS servers for ofu.fi (30 minutes)

- My domain registrar allows users to use their own DNS servers.
- Because my DNS servers are hosting the authoritative zone they are in, I also needed to set glue records.
- Glue records mean that TLD can serve ip addresses (A records) of my DNS servers in addition to normal NS records. Supplying only NS records would cause circular dependency where trying to resolve ofu.fi would yield authoritative nameserver ns.ofu.fi, to resolve ns.ofu.fi one would need to first resolve ofu.fi, but this again leads back to ns.ofu.fi, and so on.
- After setting my nameserves and glue records, I needed to wait until TLD had updated it's records. 
- I can now query ofu.fi on my own workstation from my local DNS resolver:
```
% dig +nocomment ofu.fi ANY

; <<>> DiG 9.18.24 <<>> +nocomment ofu.fi ANY
;; global options: +cmd
;ofu.fi.				IN	ANY
ofu.fi.			5944	IN	SOA	ns.ofu.fi. admin.oru.fi. 2024030401 86400 10800 259200 10800
ofu.fi.			23944	IN	NS	ns.ofu.fi.
ofu.fi.			23944	IN	NS	ns2.ofu.fi.
;; Query time: 16 msec
;; SERVER: 192.168.1.1#53(192.168.1.1) (TCP)
;; WHEN: Wed Mar 06 01:30:06 EET 2024
;; MSG SIZE  rcvd: 116
```
- My VPS provider allows to set reverse DNS for their server IPs, which saves me some trouble.
- There would be still some tinkering left such as setting DNSSEC and adding proper IPv6 settings for both servers, but the current setup has to suffice for now. 

## Setting up web-server

- I want to at least be able to point web browser on my workstation to "http://ofu.fi" and get a response from web-server.
- Setting up TLS and being able to use https would nice too.

### Configuring httpd server on mail-ofu to listen port 80 (http) (20 min)

- Edited /etc/httpd.conf according to httpd.conf OpenBSD man page examples:
```
mail# cat /etc/httpd.conf
server "ofu.fi" {
    alias "www.ofu.fi"
    listen on egress port 80
    root "/htdocs/ofu.fi"
}
```
- Started httpd:
```
mail# rcctl enable httpd
mail# rcctl start httpd
httpd(ok)
```
- Edited /var/www/htdocs/ofu.fi/index.html to contain "<html><body>ok</body></html>" to test connection 
- Port 80 was opened for incoming tcp traffic in pf.conf and pf was told to reload config 
- Web browser on my PC connects http://95.217.16.28/ and shows the ok-page, which means the server is running and reponds 

### Configuring DNS to point connections to ofu.fi to web server: (20 min)

- Two records were added to /var/named/ofu.fi.zone on ns-ofu:
```
@           IN          A           95.217.16.28
www         IN          CNAME       ofu.fi.
```
- Serial on SOA record was updated to 2024030601 
- Zone file was validated and named was reloaded to propagate changes 
- Nsd on mail-ofu got notified and updated: 
```
mail# nsd-control zonestatus
zone:	ofu.fi
	state: ok
	served-serial: "2024030601 since 2024-03-06T18:33:04"
	commit-serial: "2024030601 since 2024-03-06T18:33:04"
	wait: "84245 sec between attempts"
```
- Web browser connects to "http://ofu.fi" and shows the ok-page.
- Connecting to "http://www.ofu.fi" works too 
- I reached my primary goal pretty quickly, so I have time to setup TLS too ^_^ 

### Configuring HTTPS (2 hours) 

- OpenBSD has its own ACME client called acme-client, which can retrieve, renew and revoke certificates
- I decided to use Let's Encrypt as my certificate authority
- Acme-client man pages instruct adding directory on www-server for acme challenge which CA uses to confirm that I own the domain
- I changed httpd.conf to contain:
```
server "ofu.fi" {
    alias "www.ofu.fi"
    listen on egress port 80
    root "/htdocs/ofu.fi"

    location "/.well-known/acme-challenge/*" {
	    root "/acme"
	    request strip 2
    }
}
```
- Httpd was reloaded to read the new config
- Acme-client needed to be configured. Luckily configuration example /etc/examples/acme-client.conf contained almost everything I needed to use Let's Encrypt's API 
- I copied the example to /etc/acme-client.conf and edited it to contain: 
```
mail$ doas cat /etc/acme-client.conf
authority letsencrypt {
	api url "https://acme-v02.api.letsencrypt.org/directory"
	account key "/etc/acme/letsencrypt-privkey.pem"
}

authority letsencrypt-staging {
	api url "https://acme-staging-v02.api.letsencrypt.org/directory"
	account key "/etc/acme/letsencrypt-staging-privkey.pem"
}

domain ofu.fi {
	alternative names { www.ofu.fi }
	domain key "/etc/ssl/private/ofu.fi.key"
	domain full chain certificate "/etc/ssl/ofu.fi.fullchain.pem"
	sign with letsencrypt
}
```
- After running acme-client first time, I realized I left filenames unedited from example file, so I had to fix the mistake and run it again.
- Running acme-client second time created certificate files I wanted: 
```
mail$ doas acme-client -v ofu.fi
acme-client: /etc/ssl/private/ofu.fi.key: generated RSA domain key
acme-client: https://acme-v02.api.letsencrypt.org/directory: directories
acme-client: acme-v02.api.letsencrypt.org: DNS: 172.65.32.248
acme-client: https://acme-v02.api.letsencrypt.org/acme/finalize/1605383997/250120340497: certificate
acme-client: order.status 3
acme-client: https://acme-v02.api.letsencrypt.org/acme/cert/042dd29a72a6922827c78e00dbb9f8f68f9b: certificate
acme-client: /etc/ssl/ofu.fi.fullchain.pem: created
```
- After checking example file /etc/examples/httpd.conf, I changed /etc/httpd.conf to:
```
mail$ doas cat /etc/httpd.conf
server "ofu.fi" {
    alias "www.ofu.fi"
    listen on egress port 80
    root "/htdocs/ofu.fi"

    location "/.well-known/acme-challenge/*" {
	    root "/acme"
	    request strip 2
    }

    location * {
        block return 302 "https://$HTTP_HOST$REQUEST_URI"
    }
}

server "ofu.fi" {
    alias "www.ofu.fi"
    listen on egress tls port 443
    root "/htdocs/ofu.fi"

    tls {
        certificate "/etc/ssl/ofu.fi.fullchain.pem"
        key "/etc/ssl/private/ofu.fi.key"
    }

    location "/.well-known/acme-challenge/*" {
        root "/acme"
        request strip 2
    }
}
```
- Httpd was reloaded to read the new config 
- Port 443 was opened to tcp traffic 
- Browser now connects to "https://ofu.fi" and "https://www.ofu.fi" and loads ok-page 
- Let's Encrypt certificate expires in 90 days and they recommend to renew it every 60 days.
- I might use this www-server after the assingment is ready, so I added a line to crontab (before I forget to do it):
```
~   ~   6   */2 *   acme-client -F ofu.fi && rcctl reload httpd
```
- This renews the certificate at random time on 6th of every second month
