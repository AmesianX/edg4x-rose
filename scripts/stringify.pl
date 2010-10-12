#!/usr/bin/perl
my $help = <<EOF;
Scans C++ source file(s) for "enum" definitions and generates
functions that convert enums to strings.

When invoked with "--output=FILE" the generated definitions will be
sent to FILE along with all necessary boilerplate. Without this
switch, declarations or definitions (depending on whether the --header
switch was specified) will be sent to standard output without
boilerplate.

When invoked with "--header" then function prototypes are output. If
"--output=FILE" is also specified then definitions are sent to FILE,
and forward declarations are sent to FILE with a ".h" extension
replacing the extension already present in FILE.

When invoked with "--generic" then these functions take an "int"
argument rather than an enum argument.  This is useful when you don't
want to have to include lots of header files.

The name of the stringification function is created by concatenting
"stringify" and all the parts of the canonic enumeration type name
(the stuff separated by '::' operators).  If the boundary between
parts is not a lower-case to upper-case transition then an underscore
is inserted to improve readability. For instance, the stringifier for
"foo::bar" is "stringify_foo_bar" while the stringifier for
"Disassembler::Heuristic" is "stringifyDisassemblerHeuristic".
EOF

use strict;
my %enum;			# key is enum name; values are a hash mapping value to name
my %enum_loc;			# key is enum name; value is "file:linenumber"
my @name_stack;

sub usage {
  my($arg0) = $0 =~ /([^\/]+)$/;
  return "usage: $arg0 \\
    [--generic] [--header] [--output=FILE] \\
    [--] SOURCE_FILES...\n";
}

# Useful function for printing debugging info.
sub debug {
  my($lexer) = shift;
  #print STDERR &$lexer("debug", @_);
}

# Create a lexical analyser for a C source file. We're only interested in a subset of the language, so this is quite
# a trivial lexer.
sub make_lexer {
  my($source_file) = @_;
  open SOURCE, "<", $source_file or die "$source_file: $!\n";
  my $s = join "", <SOURCE>;
  my $linenum = 1;
  close SOURCE;
  return sub {
    if (@_) {
      return "$source_file:$linenum" if $_[0] eq 'location';
      print STDERR "$source_file:$linenum: ", join(": ", @_), "\n";
      exit 1 if $_[0] eq 'fatal';
      return;
    }
    while (1) {
      $s =~ /\G$/cgs and return undef;
      $s =~ /\G[ \t]+/cg and next;	# skip over white space
      $s =~ /\G\n/cg and do {$linenum++; next};
      $s =~ /\G\/\/.*?(\n|$)/cgs and do {$linenum++; next}; # skip over C++ comments
      $s =~ /\G(\/\*.*?(\*\/|$))/cgs and do {local $_=$1; $linenum+=tr/\n/\n/; next}; # skip over C comment
      $s =~ /\G(#\s*(include|define|undef|if(def)?|elif|else|endif)(.*?\\\n)*(.*?)\n)/cg and
	do {local $_=$1; $linenum+=tr/\n/\n/; next}; # skip CPP directives
      $s =~ /\G("(\\[0-7]{1,3}|\\.|[^"\\])*")/cgs and
	do {local $_=$1; $linenum+=tr/\n/\n/; return $1};
      $s =~ /\G('(\\[0-7]{1,3}|\\.|[^'\\])*')/cgs and return $1;
      $s =~ /\G([a-z_A-Z]\w+)/cg and return $1;
      $s =~ /\G([-+]?(0x[\da-fA-F]+|0[0-7]+|0|\d+))/cg and return $1; # integers
      $s =~ /\G(::)/cg and return $1; # so as not to be confused with ':'
      $s =~ /\G(.)/cgs and return $1;
    }
  }
}

sub canonic_name {
  return join "::", grep {$_} @name_stack, @_;
}

# Generate the name of an enum-to-string function from a canonic enum name.
sub stringify_name {
  my($enum_name) = @_;
  my $retval = "stringify";
  for (split /::/, $enum_name) {
    $retval .= "_" if (($retval=~/[A-Z]$/ && /^[A-Z]/) || ($retval=~/[a-z0-9]$/ && /^[a-z]/));
    $retval .= $_;
  }
  return $retval;
}

# Parse the beginning of a 'namespace' definition, through the opening brace. We've already consumed the word "namespace"
sub parse_namespace {
  my($lexer) = @_;
  my($ns_name) =&$lexer();
  if ('{' eq $ns_name) {
    push @name_stack, undef;
  } elsif ($ns_name =~ /^[a-z_A-Z]\w*$/ && '{' eq &$lexer()) {
    push @name_stack, $ns_name;
  }
}

# Parse the beginning of a 'struct' or 'class' definition, through the opening brace. We've already consumed the keyword.
sub parse_class {
  my($lexer) = @_;
  my($token) = &$lexer();
  if ($token eq '{') {
    push @name_stack, undef;
  } elsif ($token =~ /^[a-z_A-Z]\w*$/) {
    return parse_class($lexer) if $token eq 'QCE_EXPORT'; # for some files in roseExtensions/qtWidgets
    my $class_name = $token;
    $token = &$lexer();
    if ($token eq ':') {
      while (defined($token) && $token ne '{') {
	$token = &$lexer();
      }
    }
    push @name_stack, $class_name if $token eq '{'; # otherwise it's a use, not definition
  }
}

# Parse an enum definition through the opening brace. We've already consumed the 'enum' keyword.
sub parse_enum {
  my($lexer) = @_;
  my($token,$enum_name) = &$lexer();

  # Check for anonymous enumeration types
  if ($token eq '{') {
    push @name_stack, undef; # popped when we see corresponding '}'
    return;
  } else {
    $enum_name = $token;
    $token = &$lexer();
  }

  return if $token ne '{'; # must be a usage rather than a definition

  $enum_name = canonic_name $enum_name;
  if ($enum{$enum_name}) {
    &$lexer("error", "enum is multiply defined, this one ignored", $enum_name);
    print STDERR $enum_loc{$enum_name}, ": previous definition is here\n";
  }
  $enum{$enum_name} = {};
  $enum_loc{$enum_name} = &$lexer("location");
  my($next_value,%forward) = 0;
  while (1) {
    my($member_name,$member_value) = &$lexer();
    last if $member_name eq '}'; # ignore ",}" sequence since trailing commas are optional
    &$lexer("expected enum member name") unless $member_name =~ /^[a-z_A-Z]\w*$/;
    $token = &$lexer();
    if ($token eq '=') {
      my(@tokens);
      while (($token=&$lexer()) ne ',' && $token ne '}') {
	$token = $forward{$token} if $token=~/^[a-z_A-Z]\w*$/ && exists $forward{$token};
	push @tokens, $token;
      }
      $member_value = eval join " ", @tokens;
      &$lexer("warning", "enum member value must be an integer constant", join " ", @tokens) unless defined $member_value;
    } else {
      $member_value = $next_value;
    }
    if (exists $enum{$enum_name}{$member_value}) {
      &$lexer("warning", "enum member \"$member_name\" duplicates \"$enum{$enum_name}{$member_value}\" and will be ignored.");
    }
    $enum{$enum_name}{$member_value} = $member_name;
    $forward{$member_name} = $member_value;
    last if $token eq '}';
    &$lexer("expected ',' but got '$token'") unless $token eq ',';
    $next_value = $member_value+1;
  }
}

# Emit forward declarations
sub output_decl {
  my($generic,$output_name) = @_;
  if ($output_name) {
    open OUTPUT, ">", $output_name or die "$output_name: $!\n";
    print OUTPUT "// DO NOT EDIT -- This file was generated by $0.\n\n";
    my($const) = $output_name =~ /([^\/]+)$/;
    $const =~ s/\W/_/g;
    $const = "ROSE_" . uc $const;
    print OUTPUT "#ifndef $const\n";
    print OUTPUT "#define $const\n\n";
    print OUTPUT "#include <string>\n\n";
  } else {
    local(*OUTPUT) = *STDOUT;
    print OUTPUT "// DO NOT EDIT -- These declarations were automatically generated by $0.\n" if keys %enum;
  }
  for my $name (sort {$a cmp $b} keys %enum) {
    my $type = $generic ? "int" : $name;
    print OUTPUT "std::string ", stringify_name($name), "($type n, const char *strip=NULL, bool canonic=false);\n";
  }
  if ($output_name) {
    print OUTPUT "\n#endif\n";
    close OUTPUT;
  }
}

# Emit definitions
sub output_defn {
  my($generic,$output_name) = @_;
  if ($output_name) {
    open OUTPUT, ">", $output_name or die "$output_name: $!\n";
    print OUTPUT "// DO NOT EDIT -- This file was generated by $0.\n\n";
    my($header) = $output_name =~ /([^\/]+)$/;
    $header =~ s/\.[^\.]$/.h/;
    print OUTPUT "#include \"$header\"\n";
    print OUTPUT "#include <cassert>\n";
    print OUTPUT "#include <cstring>\n";
  } else {
    local(*OUTPUT) = *STDOUT;
  }

  for my $name (sort {$a cmp $b} keys %enum) {
    my $type = $generic ? "int" : $name;
    print OUTPUT <<"EOF";

// DO NOT EDIT -- This function was automatically generated by $0.
/** Converts an enum of type $name to a string.
 *
 *  If the supplied value is not a member of the enumeration type then the returned string will look like a C type cast but
 *  no warning will be generated.  If the \@p canonic argument is true (default is false) then the returned string will
 *  include the canonic name of the enumeration type.  If the \@p strip argument is non-null and the enum member name begins
 *  with those letters, then those letters are removed from the name. */
std::string
@{[stringify_name($name)]}($type n, const char *strip/*=NULL*/, bool canonic/*=false*/)
{
    std::string retval;
    switch (n) {
EOF
    for my $v (sort {$a <=> $b} keys %{$enum{$name}}) {
      print OUTPUT "        case $v: retval = \"$enum{$name}{$v}\"; break;\n";
    }
print OUTPUT <<"EOF"
    }
    if (retval.empty()) {
        char buf[@{[length($name)+64]}];
        int nprint = snprintf(buf, sizeof buf, \"(${name})\%d\", n);
        assert(nprint < (int)sizeof buf);
        retval = buf;
    } else {
        if (strip && !strncmp(strip, retval.c_str(), strlen(strip)))
            retval = retval.substr(strlen(strip));
        if (canonic)
            retval = \"${name}::\" + retval;
    }
    return retval;
}
EOF
  }

  close OUTPUT if $output_name;
}

###############################################################################################################################
# Main body
###############################################################################################################################

# Parse command-line
my($do_decls,$use_int,$defn_output,$decl_output);
while ($ARGV[0] =~ /^-/) {
  if ($ARGV[0] eq '--') {
    shift;
    last;
  } elsif ($ARGV[0] =~ /^(-h|-\?|--help)$/) {
    print usage, "\n", $help;
    exit 0;
  } elsif ($ARGV[0] =~ /^--header$/) {
    $do_decls = shift @ARGV;
  } elsif ($ARGV[0] =~ /^--generic$/) {
    $use_int = shift @ARGV;
  } elsif ($ARGV[0] =~ /^--output=(.*)/) {
    shift @ARGV;
    $defn_output=$1;
  } else {
    die usage;
  }
}
if ($defn_output) {
  $decl_output = $defn_output;
  $decl_output =~ s/\.[^\.]+$/.h/;
}

# Scan C++ files to generate %enum hash
for my $source_file (@ARGV) {
  my($lexer) = make_lexer $source_file;
  my($token);
  while (defined($token=&$lexer())) {
    debug $lexer, "token [$token]";
    if ($token eq '{') {
      push @name_stack, undef;
      debug $lexer, "enter [".join(" ",map{$_||"X"} @name_stack)."]";
    } elsif ($token eq '}') {
      pop @name_stack;
      debug $lexer, "leave [".join(" ",map{$_||"X"} @name_stack)."]";
    } elsif ($token eq 'namespace') {
      parse_namespace $lexer;
    } elsif ($token eq 'class' || $token eq 'struct') {
      parse_class $lexer;
    } elsif ($token eq 'enum') {
      parse_enum $lexer;
    }
  }

  # Curly braces should balance out.  If this script is being run on non-CPP processed input then it will have
  # ignored CPP directives. But sometimes people put things in "#if 0" and aren't careful about balancing the
  # braces. For instance, I've seen code like this:
  #
  #     #if 0
  #         if (x) {
  #     #endif
  #         if (y) {
  #
  # This causes problems for some other tools too. For instance, what is the proper indentation? XEmacs will
  # say that the second 'if' statement should be indented one additional level because it appears inside another
  # 'if' statement that could be enabled at a later time.
  #
  # If we find unbalanced braces, then reset to the initial state at the end of each source file.
  if (@name_stack) {
    &$lexer("unbalanced braces", "[".join(" ", map {$_||"anonymous"} @name_stack)."]");
    @name_stack = ();
  }
}

# Emit output
output_decl($use_int, $decl_output) if $do_decls;
output_defn($use_int, $defn_output) unless $do_decls && !$defn_output;
