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

sub free_random_block {
    my ($ops_ref, $active_ref, $sizes_ref, $live_ref) = @_;
    my $slot = int(rand(@{$active_ref}));
    my $id = $active_ref->[$slot];
    splice @{$active_ref}, $slot, 1;
    $$live_ref -= delete $sizes_ref->{$id};
    push @{$ops_ref}, "f $id";
}

sub realloc_random_block {
    my ($ops_ref, $active_ref, $sizes_ref, $live_ref, $peak_ref, $live_cap) = @_;
    my $slot = int(rand(@{$active_ref}));
    my $id = $active_ref->[$slot];
    my $old_size = $sizes_ref->{$id};

    my $new_size;
    if (rand() < 0.75) {
        $new_size = 16 + int(rand(4_080));
    } else {
        $new_size = 4_096 + int(rand(12_288));
    }

    my $available = $live_cap - ($$live_ref - $old_size);
    if ($new_size > $available) {
        $new_size = $available > 0 ? $available : $old_size;
    }
    $new_size = 1 if $new_size < 1;

    $$live_ref += ($new_size - $old_size);
    $$peak_ref = $$live_ref if $$live_ref > $$peak_ref;
    $sizes_ref->{$id} = $new_size;
    push @{$ops_ref}, "r $id $new_size";
}

sub flush_active_blocks {
    my ($ops_ref, $active_ref, $sizes_ref, $live_ref) = @_;
    free_random_block($ops_ref, $active_ref, $sizes_ref, $live_ref) while @{$active_ref};
}

sub generate_realloc_heavy_trace {
    srand(79);

    my @ops;
    my @active;
    my %sizes;
    my $live = 0;
    my $peak = 0;
    my $next_id = 0;
    my $live_cap = 13_000_000;

    for (1 .. 96) {
        my $size = 64 + int(rand(1_984));
        alloc_block(\@ops, \@active, \%sizes, $next_id, $size, \$live, \$peak);
        $next_id++;
    }

    for my $step (0 .. 8_191) {
        if (!@active) {
            my $size = 64 + int(rand(1_984));
            alloc_block(\@ops, \@active, \%sizes, $next_id, $size, \$live, \$peak);
            $next_id++;
            next;
        }

        if (($next_id < 2_048) && (@active < 256) && rand() < 0.22) {
            my $size = 32 + int(rand(4_064));
            if ($live + $size > $live_cap) {
                free_random_block(\@ops, \@active, \%sizes, \$live);
                next;
            }
            alloc_block(\@ops, \@active, \%sizes, $next_id, $size, \$live, \$peak);
            $next_id++;
            next;
        }

        if ((@active > 48) && rand() < 0.18) {
            free_random_block(\@ops, \@active, \%sizes, \$live);
            next;
        }

        realloc_random_block(\@ops, \@active, \%sizes, \$live, \$peak, $live_cap);
    }

    while ($next_id < 2_048) {
        my $size = 32 + int(rand(2_016));
        if (@active && ($live + $size > $live_cap)) {
            free_random_block(\@ops, \@active, \%sizes, \$live);
            next;
        }
        alloc_block(\@ops, \@active, \%sizes, $next_id, $size, \$live, \$peak);
        $next_id++;
    }

    flush_active_blocks(\@ops, \@active, \%sizes, \$live);
    write_trace('stress-realloc-heavy-s79-bal.rep', $next_id, \@ops, $peak);
}

generate_realloc_heavy_trace();
