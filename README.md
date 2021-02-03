Tools for fuzz testing PostgreSQL's COPY FROM parser
----------------------------------------------------

- copytester: A utility to run a corpus of inputs against a running server

- hongfuzz-pg.patch: patch to allow using honggfuzz fuzzer to test COPY FROM

- gencopyfuzz

- prepared corpus of different inputs, generated manually and by honggfuzz

Using copytester
----------------

Usage:

    copytester <inputdir> <connstring>

copytester connects to a running PostgreSQL server, and issues a COPY FROM command
to load each file in <inputdir> to a temporary table. It prints out any
errors, and the final contents of the table. This is useful for comparing
behavior of two PostgreSQL versions, or a patched server against unpatched one.

There are a bunch of input files included in the 'corpus' directory, and you can
use the included 'gencopyfuzz' program or honggfuzz to generate more.

For example, you can run copytester against two servers and check if the produce
the same result:

    copytester corpus "dbname=postgres port=5432" > results-A.txt
    copytester corpus "dbname=postgres port=5433" > results-B.txt
    diff -u results-A.txt results-B.txt


Using gencopyfuzz
-----------------

1. make
2. ./gencopyfuzz corpus

This generates files named 'gencopyfuzz-[00000-77777]' in the corpus directory.

Using honggfuzz
---------------

1. Apply honggfuzz-pg.patch to PostgreSQL sources:

   cd <PostgreSQL source tree>
   patch -p1 < honggfuzz-pg.patch

2. Create a test cluster following the intructions in startfuzz.sh

3. Run hongfuzz:

   ./startfuzz.sh

The 'corpus' directory in the git repository contains test inputs that
were generated with this method. If you just want to run the existing
tests against a running server, you don't need to run honggfuzz yourself.

Corpus
------

The 'corpus' directory contains test input files for COPY FROM. a few
of them were created by hand, the rest were generated by honggfuzz.
Run 'gencopyfuzz corpus' to generate another set of inputs.

The existing corpus was generated with UTF-8 as the client and server
encoding. To test other encodings and encoding conversions, you may
want to edit the dictionary in gencopyfuzz.c, and also run honggfuzz
yourself with different settings.


Tips
----

By default, copytester sends the input file to the server one byte at a time.
That's highly inefficient, but useful for finding bugs in the server's
handling of look-ahead and buffer boundaries. You can adjust the RAW_BUF_SIZE
constant if you don't want that.

Similarly, it can be very useful to reduce the server's input buffer size,
by changing the RAW_BUF_SIZE constant in src/include/commands/copyfromparse_internal.h
in the PostgreSQL source tree.
