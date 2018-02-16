#!/usr/local/bin/perl

if (!-x "findoffsets") {
  die "can't execute `findoffsets'";
}

opendir(DIR, ".") || die "can't open `.'";
@pc_files = grep(/\.pc$/, readdir DIR);
closedir DIR;

if (!@pc_files) {
  die "no `.pc' files in this directory";
}


# run `findoffsets'
system("./findoffsets > offsets.txt.new");

# compare against old offsets, exit if same
if (-r "offsets.txt" && system("cmp -s offsets.txt offsets.txt.new") == 0) {
  unlink("offsets.txt.new");
  system("touch offsets.txt");
  exit(0);
}

# move old offsets to new
unlink("offsets.txt");
rename("offsets.txt.new", "offsets.txt");

# read offsets
open(IN, "offsets.txt") || die("can't read `offsets.txt'");
while (<IN>) {
  if (/^([\w-_.]+)$/) {
    $section = $1;
    $map{$section} = {} if !exists($map{$section});
    $data = $map{$section};
  } elsif (/^field ([\w-_]+)(.*;)$/) {
    die "crap before section!" if !$section;
    $data->{$1} = $2;
  } elsif (/\S/) {
    die "unknown crap $_";
  }
}
close IN;

# modify .pc files
$bad = 0;

sub change_section ($) {
  my($sec) = @_;
  my($data) = ($map{$sec});
  my($next_class);
  
  while (<IN>) {
    if (/^(\s+field )([\w-_]+)(.*?;)(.*)$/s
	&& exists($data->{$2})) {
      if ($data->{$2} ne $3) {
	$changed = 1;		# global
	$_ = $1 . $2 . $data->{$2} . $4;
      }
      delete $data->{$2};
    } elsif (/^(module|class) ([\w-_.]+)\s*(:>|\{|has|$)/) {
      print OUT;
      $next_class = $2;
      last;
    }
    print OUT;
  }
  
  foreach $x (keys %$data) {
    $bad = 1;
    print STDERR "field $sec.$x NOT REPLACED\n";
  }

  delete $map{$sec};

  if ($next_class && $map{$next_class}) {
    change_section($next_class);
  }
}

foreach $file (@pc_files) {
  open(IN, $file) || die "can't open `$file'";
  open(OUT, ">$file.new") || die "can't write `$file.new'";
  $changed = 0;			# global
  
  while (<IN>) {
    if (/^(module|class) ([\w-_.]+)\s*(:>|\{|has|$)/ && $map{$2}) {
      print OUT;
      change_section($2);
    } else {
      print OUT;
    }
  }
  
  close IN;
  close OUT;
  
  if ($changed) {
    unlink($file);
    rename("$file.new", $file);
  } else {
    unlink("$file.new");
  }
}

foreach $x (keys %map) {
  $bad = 1;
  print STDERR "module $x NOT FOUND\n";
}

if ($bad) {
  unlink("offsets.txt");	# don't allow make to progress
}
exit($bad);
