#!/bin/bash

set -e -u

cd ~/github
git pull
cd -
bash "$(dirname "${BASH_SOURCE}")/sync.sh" || true
{
  echo 'HTTP/1.0 200 OK'
  echo 'Content-Type: text/html'
  echo
  echo 'OK'
  sleep 10
} | nc -l 18080
