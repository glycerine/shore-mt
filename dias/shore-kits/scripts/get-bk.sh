#!/bin/bash

DATFILE=$1

#NOLIST=_1cHsdesc
NOLIST=


# Latching
COUNT=0
WHAT[$COUNT]=Latching
YES[$COUNT]=__1cHlatch_t
AND[$COUNT]=
NO[$COUNT]=


# Heap Page Latching
((COUNT++))
WHAT[$COUNT]=HeapLatch
YES[$COUNT]=__1cHlatch_t
AND[$COUNT]=__1cGfile_p
NO[$COUNT]=

# BTree Page Latching
((COUNT++))
WHAT[$COUNT]=BTreeLatch
YES[$COUNT]=__1cHlatch_t
AND[$COUNT]=__1cHbtree_m
NO[$COUNT]=

# locking
NOLIST="$NOLIST ${YES[$COUNT]}"
((COUNT++))
WHAT[$COUNT]=Locking
YES[$COUNT]=__1cGlock_m
AND[$COUNT]=
NO[$COUNT]=$NOLIST

# logging
NOLIST="$NOLIST ${YES[$COUNT]}"
((COUNT++))
WHAT[$COUNT]=Logging
YES[$COUNT]="__1cIlog_core __1cFlog_m"
AND[$COUNT]=
NO[$COUNT]=$NOLIST

# Xct management
NOLIST="$NOLIST ${YES[$COUNT]}"
((COUNT++))
WHAT[$COUNT]=TxMgt
YES[$COUNT]=__1cFxct_t
AND[$COUNT]=
NO[$COUNT]=$NOLIST

# BPool
NOLIST="$NOLIST ${YES[$COUNT]}"
((COUNT++))
WHAT[$COUNT]=BPool
YES[$COUNT]=__1cEbf_m
AND[$COUNT]=
NO[$COUNT]=$NOLIST

# BTree
NOLIST="$NOLIST ${YES[$COUNT]}"
((COUNT++))
WHAT[$COUNT]=B+Tree
YES[$COUNT]=__1cHbtree_m
AND[$COUNT]=
NO[$COUNT]=$NOLIST

# SM
NOLIST="$NOLIST ${YES[$COUNT]}"
((COUNT++))
WHAT[$COUNT]=SM
YES[$COUNT]=__1cEss_m
AND[$COUNT]=
NO[$COUNT]=$NOLIST

# DORA
NOLIST="$NOLIST ${YES[$COUNT]}"
((COUNT++))
WHAT[$COUNT]=DORA
YES[$COUNT]=__1cEdora
AND[$COUNT]=
NO[$COUNT]=$NOLIST

# Kits - possibly xct logic
NOLIST="$NOLIST ${YES[$COUNT]}"
((COUNT++))
WHAT[$COUNT]=Kits
YES[$COUNT]=
AND[$COUNT]=
NO[$COUNT]=$NOLIST

# Grand total
NOLIST="$NOLIST ${YES[$COUNT]}"
((COUNT++))
WHAT[$COUNT]=TOTAL
YES[$COUNT]=
AND[$COUNT]=
NO[$COUNT]="sdesc"

function filter() {
    flag=$1
    shift
    val=$(echo $* | sed 's; [ ]*;\\|;g')
    if [ -n "$val" ]; then
	echo "ggrep $flag ' \($val\)'"
    else
	echo cat
    fi
}

for ((i=0; i <= COUNT; i++)); do
    yes=$(filter -e ${YES[$i]})
    and=$(filter -e ${AND[$i]})
    no=$(filter -v ${NO[$i]})
    CMD="cat $DATFILE | grep -v client | $yes| $and | $no | dtopk | head -n1 | awk '{print \$1}'"
    echo "$CMD" >&2
    echo -n "${WHAT[$i]} "
    bash -c "$CMD"
done