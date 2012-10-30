# --------------------------------------------------------------- #
# -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- #
# -- University of Wisconsin-Madison, subject to the terms     -- #
# -- and conditions given in the file COPYRIGHT.  All Rights   -- #
# -- Reserved.                                                 -- #
# --------------------------------------------------------------- #

proc st {func args} {

    global env
    global in_st_trans

    set del_pos 0
    set rid_pos 1
    set chk_pos 2
    set len_pos 3
    set ch_pos 4
    set fid_pos 5

    set ret ""
#    echo "func = " $func
#    echo "args = " $args
    case $func in {
	restart {
	    set ret [sm restart]
	    ovt abort_xct		
	}
	show_tmpovt {
	    if { [llength $args] != 1 } {
		error "wrong # args; should be \"fid\""
	    }
	    set ovt_info [ovt tmpovt [lindex $args 0]]
	    foreach i $ovt_info {
		echo $i
	    }
	}
	show_permovt {
	    if { [llength $args] != 1 } {
		error "wrong # args; should be \"fid\""
	    }
	    set ovt_info [ovt permovt [lindex $args 0]]
	    foreach i $ovt_info {
		echo $i
	    }
	}
	create_vol {
	    set ret [eval "[concat sm create_vol $args]"]

	    if { [lsearch [array names env] STOVT_DIR] == -1 } {
		set ovtdir [pwd]
	    } else {
		set ovtdir $env(STOVT_DIR)
	    }

	    foreach i [glob -nocomplain $ovtdir/*.dir $ovtdir/*.pag] {
		exec /bin/rm -f $i
	    }

	    #
	    # reopen a new permdb file
	    #
	    ovt newovt	
	}
	validate_file {
	    if {$in_st_trans == 0} {
		echo "*** NOT INSIDE ST TRANSACTION ***"
		return $ret
	    }
	    foreach i $args {
	    	eval "[concat st validate_rec [sm scan_rids $i]]"

		#
		#  For each entry in tmp ovt that belongs to this file $i,
		#  verify the record.
		#
		set tmpovt_info [ovt tmpovt $i]
		set tmp_rids {}
		foreach j $tmpovt_info {
		    set rid [lindex $j $rid_pos] 
		    lappend tmp_rids $rid
		    set ovt_chk [lindex $j $chk_pos] 
		    set ovt_len [lindex $j $len_pos] 

		    set data [sm read_rec_body $rid]
		    set chk [ovt chksum 0 $data]
		    set len [string length "$data"]

		    if { $len != $ovt_len } {
		        error "Validation Error: Length Mismatch for REC $rid\n\
		                 Record Length $len\tOVT Length: $ovt_len"
		    }

		    if { $chk != $ovt_chk } {
		        error "Validation Error: Checksum Mismatch for REC $rid\n\
		              Recomputed Checksum $chk\tOVT Checksum: $ovt_chk"
		    }
		}

		#
		#  For each entry in perm ovt that belongs to this file $i,
		#  if the entry is not in tmp ovt (valid or marked as removed),
		#  verify it.
		#
		set permovt_info [ovt permovt $i]
		foreach k $permovt_info {
		    set rid [lindex $k $rid_pos] 
		    set ovt_chk [lindex $k $chk_pos] 
		    set ovt_len [lindex $k $len_pos] 

		    if { [lsearch $tmp_rids $rid] >= 0 } { continue } 

		    set od [ovt peek $k]
		    if { [lindex $od $del_pos] == "d" } { continue }

		    set data [sm read_rec_body $rid]
		    set len [string length "$data"]
		    set chk [ovt chksum 0 $data]

		    if { $len != $ovt_len } {
		        error "Validation Error: Length Mismatch for REC $rid\n\
		                 Record Length $len\tOVT Length: $ovt_len"
		    }

		    if { $chk != $ovt_chk } {
		        error "Validation Error: Checksum Mismatch for REC $rid\n\
		              Recomputed Checksum $chk\tOVT Checksum: $ovt_chk"
		    }
		}
	    }
	}
	validate_rec {
	    if {$in_st_trans == 0} {
		echo "*** NOT INSIDE ST TRANSACTION ***"
		return $ret
	    }
	    foreach i $args {
		set data [sm read_rec_body $i]
		set chk [ovt chksum 0 $data] 
		set len [string length "$data"] 
		set od [ovt peek $i]
		set ovt_chk [lindex $od $chk_pos]
		set ovt_len [lindex $od $len_pos]
		case "[lindex $od $del_pos]" in {
		  "u" { error "ovt shows no record $i" }
		  "d" { error "ovt shows record $i as removed" }
		  "-" {
		        if { $len != $ovt_len } {
		        error "Validation Error: Length Mismatch for REC $i\n\
		                 Record Length $len\tOVT Length: $ovt_len"
			}

			if { $chk != $ovt_chk } {
		        error "Validation Error: Checksum Mismatch for REC $i\n\
		              Recomputed Checksum $chk\tOVT Checksum: $ovt_chk"
		        }
		      }
		}
	    }
	}
	destroy_file {
	    if {$in_st_trans == 0} {
		echo "*** NOT INSIDE ST TRANSACTION ***"
		return $ret
	    }
	    foreach i $args {
		set ret [sm destroy_file $i]	    
		foreach r [sm scan_rids $i] { st destroy_rec $r	}
	    }
	}
	begin_xct  {
	    set ret [sm begin_xct]
	    ovt begin_xct
	    set in_st_trans 1		
	}
	commit_xct {
	    set ret [sm commit_xct]
	    if {$in_st_trans == 0} {
		echo "*** NOT INSIDE ST TRANSACTION ***"
		return $ret
	    }
	    ovt commit_xct
	    set in_st_trans 0		
	}
	abort_xct {
	    set ret [sm abort_xct]
	    if {$in_st_trans == 0} {
		echo "*** NOT INSIDE ST TRANSACTION ***"
		return $ret
	    }
	    ovt abort_xct
	    set in_st_trans 0		
	}
	create_rec {
	    if {$in_st_trans == 0} {
		echo "*** NOT INSIDE ST TRANSACTION ***"
		return $ret
	    }
	    case [llength $args] in {
		2 {
		    set fid [lindex $args 0]
		    set len [lindex $args 1]
		    set len_hint $len
		}
		3 {
		    set fid [lindex $args 0]
		    set len [lindex $args 1]
		    set len_hint [lindex $args 2]
		}
		default {
		    error "wrong # args; should be \"fid len [len_hint]\""
		}
	    }
	    set data "[ovt mkdata $len]"
	    set hdr [string index $data 0]
	    set ret [sm create_rec $fid $hdr $len_hint $data]
	    ovt put $ret [ovt chksum 0 $data] $len $hdr $fid
	}
	destroy_rec {
	    if {$in_st_trans == 0} {
		echo "*** NOT INSIDE ST TRANSACTION ***"
		return $ret
	    }
	    foreach i "$args" {
		set ret [sm destroy_rec $i]

		set od [ovt peek $i]
		case "[lindex $od $del_pos]" in {
		    "u" { error "ovt shows no record $i" }
		    "d" { error "ovt shows record $i as ALREADY removed" }
		    "-" { ovt pop $i }
		}
	    }
	}
	append_rec {
	    if {$in_st_trans == 0} {
		echo "*** NOT INSIDE ST TRANSACTION ***"
		return $ret
	    }
	    case [llength $args] in {
		2 {
		    set rid [lindex $args 0]
		    set len [lindex $args 1]
		}
		default {
		    error "wrong # args; should be \"rid length\""
		}
	    }

	    set od [ovt peek $rid]
	    case "[lindex $od $del_pos]" in {
		"u" { error "ovt shows no record $i" }
		"d" { error "ovt shows record $i as removed" }
	    }	

	    set chk [lindex $od $chk_pos]
	    set olen [lindex $od $len_pos]
	    set ch [lindex $od $ch_pos]
	    set fid [lindex $od $fid_pos]

	    set data [ovt mkdata $len $ch $olen]
	    set delta [ovt chksum $olen $data]
	    set ret [sm append_rec $rid $data]
	    set chk [expr {$chk ^ $delta}]

	    ovt put $rid $chk [expr {$olen + $len}] $ch $fid
	}
	update_rec {
	    if {$in_st_trans == 0} {
		echo "*** NOT INSIDE ST TRANSACTION ***"
		return $ret
	    }
	    if { [llength $args] != 3 } {
	    	error "wrong # args; should be \"rid start length\""
	    }
	    set rid [lindex $args 0]
	    set start [lindex $args 1]
	    set len [lindex $args 2]
	    set data "[ovt mkdata $len]"
	    set body [sm read_rec_body $rid]

	    set ret [sm update_rec $rid $start $data] 

	    set od [ovt peek $rid]
	    case "[lindex $od $del_pos]" in {
		"u" { error "ovt shows no record $i" }
		"d" { error "ovt shows record $i as removed" }
	    }	

	    set olen [lindex $od $len_pos]
	    set ch [lindex $od $ch_pos]
	    set fid [lindex $od $fid_pos]
	    set chk [lindex $od $chk_pos]

	    set replaced_data [
		string range $body $start [expr { $start + $len - 1 }]]
	    set delta [ovt chksum $start $replaced_data]
	    set alpha [ovt chksum $start $data]
	    set chk [expr { $chk ^ $delta ^ $alpha }]

	    if { $start == 0 } { set ch [string index "$data" 0] }

	    ovt put $rid $chk $olen $ch $fid
	}
	truncate_rec {
	    if {$in_st_trans == 0} {
		echo "*** NOT INSIDE ST TRANSACTION ***"
		return $ret
	    }
	    case [llength $args] in {
		2 {
		    set rid [lindex $args 0]
		    set len [lindex $args 1]
		}
		default {
		    error "wrong # args; should be \"rid length\""
		}
	    }
	    set ret [sm truncate_rec $rid $len]

	    set od [ovt peek $rid]
	    case "[lindex $od $del_pos]" in {
		"u" { error "ovt shows no record $i" }
		"d" { error "ovt shows record $i as removed" }
	    }	

	    set chk [lindex $od $chk_pos]
	    set olen [lindex $od $len_pos]
	    set ch [lindex $od $ch_pos]
	    set fid [lindex $od $fid_pos]

	    set data "[ovt mkdata -$len $ch $olen]"
	    set delta [ovt chksum [expr {$olen - $len}] $data]
	    set chk [expr {$chk ^ $delta}]
	    ovt put $rid $chk [expr {$olen - $len}] $ch $fid
	}
	scan_recs {
	    if {$in_st_trans == 0} {
		echo "*** NOT INSIDE ST TRANSACTION ***"
		return $ret
	    }
	    if { [llength $args] != 1 } {
		error "wrong # args; should be \"fid\""
	    }
	    foreach r [sm scan_rids $args] {
		set res [sm read_rec $r 0 0]
		echo $res
	    }
	}
	print_scan_recs {
	    if {$in_st_trans == 0} {
		echo "*** NOT INSIDE ST TRANSACTION ***"
		return $ret
	    }
	    if { [llength $args] != 1 } {
		error "wrong # args; should be \"fid\""
	    }
	    foreach r [sm scan_rids $args] {
		sm print_rec $r 0 0
		echo ""
	    }
	}
	default {
	    set ret [sm $func $args]
	}
    }
    return $ret
}
