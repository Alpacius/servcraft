#!/usr/bin/env perl

use strict;
use warnings;
use lib '..';

use Servbuild::Makemaker::C;

%Servbuild::Makemaker::C::assignments_overwritten = (
    CFLAGS => '-fPIC -c -O2 -g',
    LDFLAGS => '-shared --version-script s1_version_script -lpthread -ldl',
    TARGET => 'libs1.so.0.1.0',
);

@Servbuild::Makemaker::C::sources = (
    `ls *.c`,
);

Servbuild::Makemaker::C::generate_makefile;
