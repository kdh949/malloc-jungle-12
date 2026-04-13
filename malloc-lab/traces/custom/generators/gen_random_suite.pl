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

sub generate_random_trace {
    my (%args) = @_;
    srand($args{seed});

    my @ops;
    my @active;
    my %sizes;
    my $live = 0;
    my $peak = 0;
    my $next_id = 0;

    while ($next_id < $args{num_ids}) {
        my $must_free = @active && (($live > $args{live_cap}) || (@active > $args{active_target}));
        my $prefer_free = @active && (rand() < $args{free_bias});
        if ($must_free || $prefer_free) {
            free_random_block(\@ops, \@active, \%sizes, \$live);
            next;
        }

        my $size = 1 + int(rand($args{max_size}));
        if (@active && ($live + $size > $args{live_cap})) {
            free_random_block(\@ops, \@active, \%sizes, \$live);
            next;
        }

        alloc_block(\@ops, \@active, \%sizes, $next_id, $size, \$live, \$peak);
        $next_id++;
    }

    flush_active_blocks(\@ops, \@active, \%sizes, \$live);
    write_trace($args{filename}, $next_id, \@ops, $peak);
}

generate_random_trace(
    filename      => 'stress-random-s17-bal.rep',
    seed          => 17,
    num_ids       => 2048,
    max_size      => 4096,
    active_target => 224,
    free_bias     => 0.43,
    live_cap      => 13_000_000,
);

generate_random_trace(
    filename      => 'stress-random-s29-bal.rep',
    seed          => 29,
    num_ids       => 4096,
    max_size      => 1024,
    active_target => 384,
    free_bias     => 0.48,
    live_cap      => 13_000_000,
);
