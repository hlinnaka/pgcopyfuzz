#!/bin/bash
#
# Start honggfuzz.
#
# You need to apply honggfuzz.patch to the server first, and configure
# with CC=hfuzz-clang
#
# Before running:
#
#   1. Create a test cluster with:
#
#      initdb -D pgdata-fuzz
#
#   2. Add "fsync=off" to pgdata-fuzz/postgresql.conf. No point in
#      stressing the disk with this.
#
#   3. Create a test table with:
#
#      CREATE TABLE copytest (t text)
#

# --keep_output generates a lot of noise, but it's really useful to see
# that it's working correctly. You may want to remove it once you're
# convinced that you've started it right.
#
# -n1 tells hongfuzz to use a single thread. That's needed because
# you cannot start postgres twice on the same data directory.

honggfuzz --keep_output --dict=fuzz-dict.txt --exit_upon_crash -n1 -P -i corpus -- \
	  postgres --single --fuzz -D pgdata-fuzz  postgres
