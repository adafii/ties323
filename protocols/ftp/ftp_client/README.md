Example of usage and debug log from stderr

``` shell
$ ./ftpc anonymous anonymous ftp.acc.umu.se 21 2> debug.txt
ftp> help
Commands:
ls - list remote files
cat - output remote file to stdout
help - this help
quit - disconnect and quit
ftp> ls
-rw-r--r--    1 ftp      ftp          1597 Sep 12  2022 HEADER.html
lrwxrwxrwx    1 ftp      ftp             3 Mar 16  2010 Public -> pub
drwxr-xr-x    3 ftp      ftp            16 Oct 24 07:15 about
drwxr-sr-x   24 ftp      ftp            27 Jan 28 13:53 cdimage
drwxr-xr-x    2 ftp      ftp             3 Jun 14  2006 conspiracy
lrwxrwxrwx    1 ftp      ftp            22 Mar 16  2010 debian -> cdimage/.debian-mirror
lrwxrwxrwx    1 ftp      ftp            16 Mar 16  2010 debian-cd -> cdimage/release/
-rw-r--r--    1 ftp      ftp         15086 Apr 02  2018 favicon.ico
lrwxrwxrwx    1 ftp      ftp             7 Mar 30  2021 images -> cdimage
drwxr-xr-x   92 ftp      ftp            98 Dec 18 18:36 mirror
drwxr-xr-x    4 ftp      ftp            12 Dec 17  2019 pub
lrwxrwxrwx    1 ftp      ftp            23 Mar 16  2010 releases -> mirror/ubuntu-releases/
-rw-r--r--    1 ftp      ftp          1920 Nov 12  2021 robots.txt
lrwxrwxrwx    1 ftp      ftp            12 Aug 01  2016 tails -> mirror/tails
lrwxrwxrwx    1 ftp      ftp            13 Mar 16  2010 ubuntu -> mirror/ubuntu
ftp> cat robots.txt
# robots.txt for http://ftp.acc.umu.se/
# This file specifies what harvesting robots are allowed to index and not.
#
# For information on the original format:
# http://www.robotstxt.org/
#
# For information on updated format that allows wildcards:
# https://developers.google.com/webmasters/control-crawl-index/docs/robots_txt

User-agent: *
# Rules without wildcards - original spec
Disallow: /cdimage/.debian-mirror
Disallow: /cdimage/snapshot
Disallow: /mirror/cdimage/.debian-mirror
Disallow: /mirror/cdimage/snapshot
Disallow: /debian/pool
Disallow: /mirror/debian/pool
Disallow: /mirror/ubuntu/pool
Disallow: /pub/debian/pool
Disallow: /ubuntu/pool
Disallow: /mirror/archlinux/pool
Disallow: /mirror/debian-multimedia/pool
Disallow: /mirror/linuxdeepin/packages/pool
Disallow: /mirror/linuxmint.com/packages/pool
Disallow: /mirror/osdn.net
Disallow: /mirror/parrotsec.org/parrot/pool
Disallow: /mirror/raspbian/mate/pool
Disallow: /mirror/raspbian/multiarchcross/pool
Disallow: /mirror/raspbian/raspbian/pool
Disallow: /mirror/solydxk.com/repository/pool
Disallow: /mirror/temp
Disallow: /mirror/trisquel/packages/pool
Disallow: /mirror/videolan.org/debian/pool
Disallow: /mirror/videolan.org/ubuntu/pool
Disallow: /mirror/opensuse.org/tumbleweed/repo
Disallow: /mirror/fedora/enchilada/linux
# Rules with wildcards - enhanced spec
Allow: /icons/*.png$
Allow: /icons2/*.png$
Disallow: /*.iso$
Disallow: /*.deb$
Disallow: /*.rpm$
Disallow: /*.gz$
Disallow: /*.bz2$
Disallow: /*.xz$
Disallow: /*.arj$
Disallow: /*.rar$
Disallow: /*.zip$
Disallow: /*.lzh$
Disallow: /*.lha$
Disallow: /*.7z$
Disallow: /*.avi$
Disallow: /*.wmv$
Disallow: /*.mpg$
Disallow: /*.mpeg$
Disallow: /*.mp4$
Disallow: /*.mkv$
Disallow: /*.flv$
Disallow: /*.qt$
Disallow: /*.mov$
Disallow: /*.m4v$
Disallow: /*.webm$
Disallow: /*.mp3$
Disallow: /*.ogg$
Disallow: /*.gif$
Disallow: /*.png$
Disallow: /*.jpg$
Disallow: /*.dmg$
Disallow: /*.zim$
ftp> quit

$ cat debug.txt
S: 220 Please use https://mirror.accum.se/ whenever possible.
C: OPTS MLST type;size;modify;UNIX.mode;UNIX.uid;UNIX.gid;
S: 501 Option not understood.
C: USER anonymous
S: 331 Please specify the password.
C: PASS anonymous
S: 230 Login successful.
C: PASV
S: 227 Entering Passive Mode (194,71,11,173,125,84).
C: LIST
S: 150 Here comes the directory listing.
S: 226 Directory send OK.
S: -rw-r--r--    1 ftp      ftp          1597 Sep 12  2022 HEADER.html
lrwxrwxrwx    1 ftp      ftp             3 Mar 16  2010 Public -> pub
drwxr-xr-x    3 ftp      ftp            16 Oct 24 07:15 about
drwxr-sr-x   24 ftp      ftp            27 Jan 28 13:53 cdimage
drwxr-xr-x    2 ftp      ftp             3 Jun 14  2006 conspiracy
lrwxrwxrwx    1 ftp      ftp            22 Mar 16  2010 debian -> cdimage/.debian-mirror
lrwxrwxrwx    1 ftp      ftp            16 Mar 16  2010 debian-cd -> cdimage/release/
-rw-r--r--    1 ftp      ftp         15086 Apr 02  2018 favicon.ico
lrwxrwxrwx    1 ftp      ftp             7 Mar 30  2021 images -> cdimage
drwxr-xr-x   92 ftp      ftp            98 Dec 18 18:36 mirror
drwxr-xr-x    4 ftp      ftp            12 Dec 17  2019 pub
lrwxrwxrwx    1 ftp      ftp            23 Mar 16  2010 releases -> mirror/ubuntu-releases/
-rw-r--r--    1 ftp      ftp          1920 Nov 12  2021 robots.txt
lrwxrwxrwx    1 ftp      ftp            12 Aug 01  2016 tails -> mirror/tails
lrwxrwxrwx    1 ftp      ftp            13 Mar 16  2010 ubuntu -> mirror/ubuntu

C: PASV
S: 227 Entering Passive Mode (194,71,11,173,105,217).
C: RETR robots.txt
S: 150 Opening BINARY mode data connection for robots.txt (1920 bytes).
S: 226 Transfer complete.
S: # robots.txt for http://ftp.acc.umu.se/
# This file specifies what harvesting robots are allowed to index and not.
#
# For information on the original format:
# http://www.robotstxt.org/
#
# For information on updated format that allows wildcards:
# https://developers.google.com/webmasters/control-crawl-index/docs/robots_txt

User-agent: *
# Rules without wildcards - original spec
Disallow: /cdimage/.debian-mirror
Disallow: /cdimage/snapshot
Disallow: /mirror/cdimage/.debian-mirror
Disallow: /mirror/cdimage/snapshot
Disallow: /debian/pool
Disallow: /mirror/debian/pool
Disallow: /mirror/ubuntu/pool
Disallow: /pub/debian/pool
Disallow: /ubuntu/pool
Disallow: /mirror/archlinux/pool
Disallow: /mirror/debian-multimedia/pool
Disallow: /mirror/linuxdeepin/packages/pool
Disallow: /mirror/linuxmint.com/packages/pool
Disallow: /mirror/osdn.net
Disallow: /mirror/parrotsec.org/parrot/pool
Disallow: /mirror/raspbian/mate/pool
Disallow: /mirror/raspbian/multiarchcross/pool
Disallow: /mirror/raspbian/raspbian/pool
Disallow: /mirror/solydxk.com/repository/pool
Disallow: /mirror/temp
Disallow: /mirror/trisquel/packages/pool
Disallow: /mirror/videolan.org/debian/pool
Disallow: /mirror/videolan.org/ubuntu/pool
Disallow: /mirror/opensuse.org/tumbleweed/repo
Disallow: /mirror/fedora/enchilada/linux
# Rules with wildcards - enhanced spec
Allow: /icons/*.png$
Allow: /icons2/*.png$
Disallow: /*.iso$
Disallow: /*.deb$
Disallow: /*.rpm$
Disallow: /*.gz$
Disallow: /*.bz2$
Disallow: /*.xz$
Disallow: /*.arj$
Disallow: /*.rar$
Disallow: /*.zip$
Disallow: /*.lzh$
Disallow: /*.lha$
Disallow: /*.7z$
Disallow: /*.avi$
Disallow: /*.wmv$
Disallow: /*.mpg$
Disallow: /*.mpeg$
Disallow: /*.mp4$
Disallow: /*.mkv$
Disallow: /*.flv$
Disallow: /*.qt$
Disallow: /*.mov$
Disallow: /*.m4v$
Disallow: /*.webm$
Disallow: /*.mp3$
Disallow: /*.ogg$
Disallow: /*.gif$
Disallow: /*.png$
Disallow: /*.jpg$
Disallow: /*.dmg$
Disallow: /*.zim$

Disconnected
```
