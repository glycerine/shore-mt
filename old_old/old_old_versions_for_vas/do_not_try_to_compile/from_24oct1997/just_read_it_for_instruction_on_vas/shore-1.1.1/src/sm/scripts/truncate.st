# --------------------------------------------------------------- #
# -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- #
# -- University of Wisconsin-Madison, subject to the terms     -- #
# -- and conditions given in the file COPYRIGHT.  All Rights   -- #
# -- Reserved.                                                 -- #
# --------------------------------------------------------------- #

#
#  This script tests record truncate. 
#
#	record:  		small, medium, large
#	truncate size:		small, medium, large
#	transaction:		abort, commit
#
if { ! [is_set RESTART] } {
    set RESTART 1
}
set_config_info
set PG_SZ $lg_rec_page_space

proc truncate_rec { fid rec_size tr_size } {
    set rec_size [format "%.0f" $rec_size]
    set tr_size [format "%.0f" $tr_size]
    puts stdout "$rec_size  $tr_size" nonewline
    set r1 [ st create_rec $fid $rec_size ]
    set r2 [ st create_rec $fid $rec_size ]

    #sm print_rec $r1 0 0 
    for { set i 1 } { $i <= 10 } { incr i } {
	set rec_size [expr {$rec_size - $tr_size}]
	if { $rec_size < 0 } { break }
	st truncate_rec $r1 $tr_size			
	st truncate_rec $r2 $tr_size			
	puts stdout "."	nonewline
    }
    verbose ""
}

source $script_dir/vol.init

st begin_xct
set f [sm create_file $volid]
verbose "\nCreate File $f\n"
st commit_xct

#
#  for records of different sizes (with respect to PG_SZ),
#  truncate different amount (with respect to PG_SZ).
#
foreach rec_sz { 0.1 0.9 0.99 1.0 2.0 10.0 50.0 100.0 } {
    foreach tr_sz { 0.01 0.1 0.5 0.9 0.99 1.0 2.0 10.0 50.0 99.0 } {
	if { $rec_sz >= $tr_sz } {
	    st begin_xct
	    truncate_rec $f [expr {$PG_SZ * $rec_sz}] [expr {$PG_SZ * $tr_sz}]
	    st abort_xct
	    if { $RESTART } { st restart }

	    st begin_xct
	    truncate_rec $f [expr {$PG_SZ * $rec_sz}] [expr {$PG_SZ * $tr_sz}]
	    st commit_xct

	    if { $RESTART } {
	      st restart
	      st begin_xct
	      truncate_rec $f [expr {$PG_SZ * $rec_sz}] [expr {$PG_SZ * $tr_sz}]
	      st restart	
	    }
	}
    }
    st begin_xct
    st validate_file $f
    st commit_xct
}

verbose "\n====================================="
verbose "Permovt Info"
verbose "-------------------------------------"
st show_permovt $f
verbose "=====================================\n"

verbose "\nFinal File Validation"
st begin_xct
st validate_file $f
st commit_xct
