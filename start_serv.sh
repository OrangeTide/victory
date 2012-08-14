#!/bin/bash
ulimit -n 4096 -s 512
exec ./serv "$@"
# ab -i -n 100000 -c 1000 http://localhost:8080/
