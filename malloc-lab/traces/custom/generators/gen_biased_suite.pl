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

sub flush_active_blocks {
    my ($ops_ref, $active_ref, $sizes_ref, $live_ref) = @_;
    free_random_block($ops_ref, $active_ref, $sizes_ref, $live_ref) while @{$active_ref};
}

sub generate_small_biased_trace {
    srand(41);

    my @ops;
    my @active;
    my %sizes;
    my $live = 0;
    my $peak = 0;
    my $live_cap = 8_000_000;

    for my $id (0 .. 4095) {
        if (@active && (($live > $live_cap) || (@active > 288) || rand() < 0.47)) {
            free_random_block(\@ops, \@active, \%sizes, \$live);
        }

        my $size = rand() < 0.85 ? (1 + int(rand(64))) : (65 + int(rand(448)));
        if (@active && ($live + $size > $live_cap)) {
            free_random_block(\@ops, \@active, \%sizes, \$live);
        }
        alloc_block(\@ops, \@active, \%sizes, $id, $size, \$live, \$peak);
    }

    flush_active_blocks(\@ops, \@active, \%sizes, \$live);
    write_trace('stress-bias-small-s41-bal.rep', 4096, \@ops, $peak);
}

sub generate_large_biased_trace {
    srand(53);

    my @ops;
    my @active_churn;
    my @anchors;
    my %sizes;
    my $live = 0;
    my $peak = 0;
    my $next_id = 0;
    my $live_cap = 13_000_000;

    for (1 .. 48) {
        my $size = 32_768 + int(rand(32_768));
        alloc_block(\@ops, \@anchors, \%sizes, $next_id, $size, \$live, \$peak);
        $next_id++;
    }

    while ($next_id < 2304) {
        if (@active_churn && (($live > $live_cap) || (@active_churn > 144) || rand() < 0.58)) {
            free_random_block(\@ops, \@active_churn, \%sizes, \$live);
            next;
        }

        my $size = rand() < 0.78 ? (16 + int(rand(240))) : (8_192 + int(rand(57_344)));
        if (@active_churn && ($live + $size > $live_cap)) {
            free_random_block(\@ops, \@active_churn, \%sizes, \$live);
            next;
        }

        alloc_block(\@ops, \@active_churn, \%sizes, $next_id, $size, \$live, \$peak);
        $next_id++;
    }

    flush_active_blocks(\@ops, \@active_churn, \%sizes, \$live);
    flush_active_blocks(\@ops, \@anchors, \%sizes, \$live);
    write_trace('stress-bias-large-s53-bal.rep', $next_id, \@ops, $peak);
}

sub generate_long_lived_trace {
    srand(97);

    my @ops;
    my @anchors;
    my @active_churn;
    my %sizes;
    my $live = 0;
    my $peak = 0;
    my $next_id = 0;
    my $live_cap = 13_000_000;

    for (1 .. 64) {
        my $size = 16_384 + int(rand(49_152));
        alloc_block(\@ops, \@anchors, \%sizes, $next_id, $size, \$live, \$peak);
        $next_id++;
    }

    while ($next_id < 3136) {
        if (@active_churn && (($live > $live_cap) || (@active_churn > 192) || rand() < 0.52)) {
            free_random_block(\@ops, \@active_churn, \%sizes, \$live);
            next;
        }

        my $size = rand() < 0.9 ? (32 + int(rand(992))) : (1_024 + int(rand(8_192)));
        if (@active_churn && ($live + $size > $live_cap)) {
            free_random_block(\@ops, \@active_churn, \%sizes, \$live);
            next;
        }

        alloc_block(\@ops, \@active_churn, \%sizes, $next_id, $size, \$live, \$peak);
        $next_id++;
    }

    flush_active_blocks(\@ops, \@active_churn, \%sizes, \$live);
    flush_active_blocks(\@ops, \@anchors, \%sizes, \$live);
    write_trace('stress-long-lived-s97-bal.rep', $next_id, \@ops, $peak);
}

generate_small_biased_trace();
generate_large_biased_trace();
generate_long_lived_trace();
