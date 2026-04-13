#!/usr/bin/perl
use strict;
use warnings;
use FindBin qw($Bin);
use File::Path qw(make_path);

my $OUT_DIR = "$Bin/../valid";
make_path($OUT_DIR);

sub write_trace {
    my ($filename, $num_ids, $ops_ref, $peak_live) = @_;
    my $path = "$OUT_DIR/$filename";
    open my $fh, '>', $path or die "cannot open $path: $!";
    my $suggested = int($peak_live * 1.10) + 256;
    print {$fh} "$suggested\n";
    print {$fh} "$num_ids\n";
    print {$fh} scalar(@{$ops_ref}) . "\n";
    print {$fh} "1\n";
    print {$fh} "$_\n" for @{$ops_ref};
    close $fh or die "cannot close $path: $!";
}

sub alloc_block {
    my ($ops_ref, $active_ref, $sizes_ref, $id, $size, $live_ref, $peak_ref) = @_;
    push @{$ops_ref}, "a $id $size";
    push @{$active_ref}, $id;
    $sizes_ref->{$id} = $size;
    $$live_ref += $size;
    $$peak_ref = $$live_ref if $$live_ref > $$peak_ref;
}

sub free_specific_block {
    my ($ops_ref, $sizes_ref, $id, $live_ref) = @_;
    $$live_ref -= delete $sizes_ref->{$id};
    push @{$ops_ref}, "f $id";
}

sub remove_id_from_active {
    my ($active_ref, $id) = @_;
    for my $i (0 .. $#{$active_ref}) {
        next if $active_ref->[$i] != $id;
        splice @{$active_ref}, $i, 1;
        return;
    }
}

sub generate_fragment_wave_trace {
    srand(67);

    my @ops;
    my @active;
    my %sizes;
    my $live = 0;
    my $peak = 0;
    my $next_id = 0;

    for my $wave (0 .. 31) {
        my @primary;
        my @fillers;
        my @merges;

        for my $slot (0 .. 23) {
            my $base = $slot % 2 ? 1_536 : 768;
            my $size = $base + int(rand(96));
            alloc_block(\@ops, \@active, \%sizes, $next_id, $size, \$live, \$peak);
            push @primary, { id => $next_id, size => $size };
            $next_id++;
        }

        for my $slot (grep { $_ % 2 == 1 } 0 .. $#primary) {
            my $id = $primary[$slot]->{id};
            remove_id_from_active(\@active, $id);
            free_specific_block(\@ops, \%sizes, $id, \$live);
        }

        for my $slot (grep { $_ % 2 == 1 } 0 .. $#primary) {
            my $size = $primary[$slot]->{size} - (48 + int(rand(80)));
            $size = 96 if $size < 96;
            alloc_block(\@ops, \@active, \%sizes, $next_id, $size, \$live, \$peak);
            push @fillers, $next_id;
            $next_id++;
        }

        for my $slot (grep { $_ % 2 == 0 } 0 .. $#primary) {
            my $id = $primary[$slot]->{id};
            remove_id_from_active(\@active, $id);
            free_specific_block(\@ops, \%sizes, $id, \$live);
        }

        for (1 .. 6) {
            my $size = 2_048 + int(rand(768));
            alloc_block(\@ops, \@active, \%sizes, $next_id, $size, \$live, \$peak);
            push @merges, $next_id;
            $next_id++;
        }

        for my $id (reverse @fillers) {
            remove_id_from_active(\@active, $id);
            free_specific_block(\@ops, \%sizes, $id, \$live);
        }

        for my $id (@merges) {
            remove_id_from_active(\@active, $id);
            free_specific_block(\@ops, \%sizes, $id, \$live);
        }
    }

    write_trace('stress-fragment-wave-s67-bal.rep', $next_id, \@ops, $peak);
}

generate_fragment_wave_trace();
