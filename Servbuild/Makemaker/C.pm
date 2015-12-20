package Servbuild::Makemaker::C;

require Exporter;
@ISA = qw(Exporter);
@Export_OK = qw(local_prefix assignments_recursive assignments_initialization assignments_concat assignments_overwritten generate_makefile);

use strict;
use warnings;

my $VARIABLE_STUB = '';

# user-defined directory prefix
our $local_prefix = '';

# user-defined recursive assignments (=)
our %assignments_recursive = (
);

# user-defined initialization assignments (?=)
our %assignments_initialization = (
);

# user-defined concat assignments (+=)
our %assignments_concat = (
);

# user-defined overwritten assignements (:=)
our %assignments_overwritten = (
    CFLAGS => $VARIABLE_STUB,
    LDFLAGS => $VARIABLE_STUB,
    TARGET => $VARIABLE_STUB,
);

my $assignments_index = [
    [\%assignments_recursive => '='],
    [\%assignments_initialization => '?='],
    [\%assignments_concat => '+='],
    [\%assignments_overwritten => ':='],
];

# user-defined sources
our @sources = (
);

sub generate_makefile {
    my $result_makefile = '';
    for my $assignments_itr (@$assignments_index) {
        my ($variables, $operator) = @$assignments_itr;
        while (my ($variable, $value) = each %$variables) {
            $result_makefile .= "$variable $operator $value\n";
        }
    }
    $result_makefile .= "\n";

    # predefined object-to-target linkage command
    my $linkage_command = "ld \$(LDFLAGS) \$^ -o $assignments_overwritten{TARGET}";

    # predefined source-to-object compilation command
    my $compilation_command = "gcc \$(CFLAGS) \$< -o \$@";

    my @source_list = split /\s+/, join(' ', @sources);
    my $object_list = '';
    my $compliation_part = '';
    for my $source_iter (@source_list) {
        my $dependencies = `gcc -MM $source_iter`;
        $compliation_part .= $dependencies . "\t$compilation_command\n\n";
        $object_list .= "$1 " if ($dependencies =~ /^((?:.*?)\.o):/);
    }
    my $linkage_part = "all: $object_list\n\t$linkage_command\n\n";

    $result_makefile .= $linkage_part;
    $result_makefile .= $compliation_part;

    # predefined clean parts
    my $clean_command = ".PHONY: clean\n\nclean:\n\trm $object_list $assignments_overwritten{TARGET}\n\n";
    $result_makefile .= $clean_command;

        my $filehandle_makefile;
        open($filehandle_makefile, '>', 'makefile') or die "build.pl: cannot open makefile: $!";
        print $filehandle_makefile $result_makefile;
        close $filehandle_makefile;
}

1;
