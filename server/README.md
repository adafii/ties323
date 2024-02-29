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


