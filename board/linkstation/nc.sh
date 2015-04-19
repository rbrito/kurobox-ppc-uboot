#! /bin/bash

[ $# = 1 ] || { echo "Usage: $0 target_ip" >&2 ; exit 1 ; }
TARGET_IP=$1

stty -icanon -echo intr ^T
#nc -u -l -p 6666 < /dev/null &
nc -u -p 6666 -v -v ${TARGET_IP} 6666
stty icanon echo intr ^C

