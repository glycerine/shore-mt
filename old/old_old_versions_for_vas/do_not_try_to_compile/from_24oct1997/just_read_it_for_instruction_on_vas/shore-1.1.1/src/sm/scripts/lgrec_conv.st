# --------------------------------------------------------------- #
# -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- #
# -- University of Wisconsin-Madison, subject to the terms     -- #
# -- and conditions given in the file COPYRIGHT.  All Rights   -- #
# -- Reserved.                                                 -- #
# --------------------------------------------------------------- #

source $script_dir/vol.init
if {$volid == 0} {
    set volid 10
    st create_vol $volid force
    set volid [sm init_vol_logical_id $volid 100]
    sm mount
}

#
#  begin transaction
#
st begin_xct
set fid [sm create_file $volid]
verbose "\nFile $fid created\n"

set r1 [st create_rec $fid 96]
verbose "Record $r1 created\n"

set r2 [st create_rec $fid 96]
verbose "Record $r2 created\n"

for { set i 1 } { $i <= 200 } { incr i } {
        st append_rec $r1 96 
        st append_rec $r2 96 
        verbose "\n$i ---------------------------------------"
        st validate_file $fid
        verbose "---------------------------------------"
        #st print_scan_recs $fid
}

st show_permovt $fid

st commit_xct
