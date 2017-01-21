#!/usr/bin/env perl

use strict;
use warnings;
use lib '..';

use Servbuild::Makemaker::C;

%Servbuild::Makemaker::C::assignments_overwritten = (
    CFLAGS => '-c -fPIC -O2 -g -std=gnu11',
    LDFLAGS => '-shared -lpthread',
    TARGET => 'libp7r.so.0.0.1',
);

@Servbuild::Makemaker::C::sources = (
    `ls *.c`,
    '../util/scraft_hashtable.c',
    '../util/scraft_rbt.c',
);

Servbuild::Makemaker::C::generate_makefile;
