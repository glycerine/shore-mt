# --------------------------------------------------------------- #
# -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- #
# -- University of Wisconsin-Madison, subject to the terms     -- #
# -- and conditions given in the file COPYRIGHT.  All Rights   -- #
# -- Reserved.                                                 -- #
# --------------------------------------------------------------- #

#
#	Test large record creates 
#
source $script_dir/vol.init

set num_rec 200
set rec_size 3000

#echo "num_rec = " $num_rec
puts stdout "num_rec = $num_rec "

st begin_xct
set fid [st create_file $volid]

set rid(0) $null_rid
echo creating $num_rec of size $rec_size
for {set i 1} {$i <= $num_rec} {incr i} {
    set rid($i) [ st create_rec $fid 3000 ]
    puts stdout "created $rid($i)"
}

puts stdout "committing $num_rec creates of size $rec_size"
st commit_xct

st begin_xct
puts stdout "validating"
st validate_file $fid
st commit_xct


unset num_rec
unset rec_size
unset rid
unset fid
