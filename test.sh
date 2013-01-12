#!/bin/zsh
set -eu
[ 'id="5"' = "$(echo '<a id="5"/>' | ./xqillac -i - <(echo '/a/@id'))" ] || echo "can't read stdin"

