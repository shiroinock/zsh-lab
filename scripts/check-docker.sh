#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
version=$(cat VERSION)
docker build --target test -t "zsh-lab-test:$version" -f container/Dockerfile .
docker run --rm --init --network=none --read-only --cap-drop=ALL \
  --security-opt=no-new-privileges --pids-limit=64 --memory=256m --memory-swap=256m --cpus=1 \
  --tmpfs /tmp:rw,nosuid,nodev,noexec,size=64m,mode=1777 \
  --tmpfs /work:rw,nosuid,nodev,size=32m,uid=1000,gid=1000,mode=0700 \
  --tmpfs /home/learner:rw,nosuid,nodev,size=16m,uid=1000,gid=1000,mode=0700 \
  --entrypoint /usr/local/libexec/test-native "zsh-lab-test:$version"
docker build --target runtime -t "zsh-lab-workshop:$version" -f container/Dockerfile .
"${1:-./zsh-lab}" --prepare
