# sample perl routine for rpost

# 3 key things
# 1. we get in the input file name
# 2. we return the output file name
# 3. You MUST have the close on both the input
#    file and output file.  Remember this is 
# running as a subroutine, not a full blown program
# and perl only closes files upon program exit, or
# reuse of file handle.

package Embed::Persistant;

sub perl_rpost {

	my $infile = $_[0];  # input file name
	my $outfile = "/tmp/tmp_rpost";

	# open files for input and output
	open IFP, "<${infile}" or die "Can't read ${infile}";
	open OFP, ">${outfile}" or die "Can't create ${outfile}";

	# now strip out what we don't need
	while ( $line = <IFP>) {
	    if($line !~ /^NNTP-Posting|^Xref/) {
		# write out to file
		print OFP $line;
	    }
	}

	close IFP;
	close OFP;

	# return output file name
	return $outfile;
}
