#!/usr/bin/env perl

use strict;
use warnings;
use lib '..';

use Servbuild::Makemaker::C;

%Servbuild::Makemaker::C::assignments_overwritten = (
    CFLAGS => '-fPIC -c -O2 -g',
    LDFLAGS => '-shared --version-script p7_version_script -lpthread',
    TARGET => 'libp7.so.0.4.0',
);

@Servbuild::Makemaker::C::sources = (
    `ls *.c`,
    '../util/scraft_hashtable.c',
    '../util/scraft_rbt.c',
);

Servbuild::Makemaker::C::generate_makefile;
