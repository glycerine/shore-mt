# --------------------------------------------------------------- #
# -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- #
# -- University of Wisconsin-Madison, subject to the terms     -- #
# -- and conditions given in the file COPYRIGHT.  All Rights   -- #
# -- Reserved.                                                 -- #
# --------------------------------------------------------------- #

#
#  This script tests record update. 
#
#	record:  		small, medium, large
#	update size:		small, medium, large
#	update location:	beginning, middle, end
#	transaction:		abort, commit
#

if { ! [is_set RESTART] } {
    set RESTART 1
}
set_config_info
set PG_SZ $lg_rec_page_space

proc update_rec { fid rec_size up_size } {
    set rec_size [format "%.0f" $rec_size]
    set up_size [format "%.0f" $up_size]

    puts stdout "$rec_size  $up_size" nonewline

    set r1 [ st create_rec $fid $rec_size ]
    set r2 [ st create_rec $fid $rec_size ]

    #
    # update at the beginning
    #
    set start 0
    st update_rec $r1 $start $up_size
    st update_rec $r2 $start $up_size
    puts stdout "." nonewline

    #
    # update in the middle 
    #
    if { $rec_size > [expr {$up_size * 2}] } {
	set start [expr {$rec_size / 2}]
    	st update_rec $r1 $start $up_size
    	st update_rec $r2 $start $up_size
        puts stdout "." nonewline
    }
	
    #
    # update at the end
    #
    if { $rec_size > [expr {$up_size * 3}] } {
	set start [expr {$rec_size - $up_size}]
    	st update_rec $r1 $start $up_size
    	st update_rec $r2 $start $up_size
        puts stdout "." nonewline
    }

    verbose ""
}

#####  main  #####
source $script_dir/vol.init

st begin_xct
set f [sm create_file $volid]
verbose "\nCreate File $f\n"
st commit_xct

#
#  for records of different sizes (with respect to PG_SZ),
#  update different amount (with respect to PG_SZ).
#
foreach rec_sz { 0.1 0.9 0.99 1.0 2.0 10.0 50.0 100.0 } {
    foreach up_sz { 0.01 0.1 0.5 0.9 0.99 1.0 2.0 10.0 50.0 99.0 } {
	if { $rec_sz > $up_sz } {
	    st begin_xct
	    update_rec $f [expr {$PG_SZ * $rec_sz}] [expr {$PG_SZ * $up_sz}]
	    st abort_xct
	    if { $RESTART } { st restart }

	    st begin_xct
	    update_rec $f [expr {$PG_SZ * $rec_sz}] [expr {$PG_SZ * $up_sz}]
	    st commit_xct
	    if { $RESTART } {
		st restart
	        st begin_xct
	        update_rec $f [expr {$PG_SZ * $rec_sz}] [expr {$PG_SZ * $up_sz}]
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
