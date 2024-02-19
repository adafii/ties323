To fix CGI in this lab, I have done the following changes:

```
In the lab root directory

$ mkdir -p server/etc/apache2/conf-available/

Edit server/etc/apache2/conf-available/cgi-enabled.conf to contain:

<Directory "/var/www/html/cgi-enabled">
    Options +ExecCGI
    AddHandler cgi-script .cgi .sh
</Directory>

$ mkdir -p server/var/www/html/cgi-enabled
$ mv server/var/www/index.html server/var/www/html/
$ cp server/usr/lib/cgi-bin/test.sh server/var/www/html/cgi-enabled/

Edit index.html to point /cgi-enabled/test.sh instead of /cgi-bin/test.sh

Add to server.startup before starting apache2
a2enmod cgid
a2enconf cgi-enabled
```

### Packet captures
- [GET](shared/get.pcap)
- [POST](shared/post.pcap)
