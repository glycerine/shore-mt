/* -*- mode:C++; c-basic-offset:4 -*-
     Shore-kits -- Benchmark implementations for Shore-MT
   
                       Copyright (c) 2007-2009
      Data Intensive Applications and Systems Labaratory (DIAS)
               Ecole Polytechnique Federale de Lausanne
   
                         All Rights Reserved.
   
   Permission to use, copy, modify and distribute this software and
   its documentation is hereby granted, provided that both the
   copyright notice and this permission notice appear in all copies of
   the software, derivative works or modified versions, and any
   portions thereof, and that both notices appear in supporting
   documentation.
   
   This code is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS
   DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
   RESULTING FROM THE USE OF THIS SOFTWARE.
*/

/** @file:   dora_tpcb_impl.cpp
 *
 *  @brief:  DORA TPCB TRXs
 *
 *  @note:   Implementation of RVPs and Actions that synthesize (according to DORA)
 *           the TPCB trxs 
 *
 *  @author: Ippokratis Pandis (ipandis)
 *  @date:   July 2009
 */

#include "dora/tpcb/dora_tpcb_impl.h"
#include "dora/tpcb/dora_tpcb.h"

using namespace shore;
using namespace tpcb;


ENTER_NAMESPACE(dora);


/******************************************************************** 
 *
 * DORA TPCB ACCT_UPDATE
 *
 ********************************************************************/

DEFINE_DORA_FINAL_RVP_CLASS(final_au_rvp,acct_update);


/******************************************************************** 
 *
 * DORA TPCB ACCT_UPDATE ACTIONS
 *
 * (1) UPD-BR
 * (2) UPD-TE
 * (3) UPD-AC
 * (4) INS-HI 
 *
 ********************************************************************/


void upd_br_action::calc_keys()
{
    _down.push_back(_in.b_id);
}


w_rc_t upd_br_action::trx_exec() 
{
    assert (_penv);
    w_rc_t e = RCOK;

    // get table tuple from the cache
    // Subscriber
    table_row_t* prb = _penv->branch_man()->get_tuple();
    assert (prb);

    rep_row_t areprow(_penv->branch_man()->ts());
    areprow.set(_penv->branch_desc()->maxsize()); 
    prb->_rep = &areprow;


    { // make gotos safe

        /* UPDATE Branches
         * SET    b_balance = b_balance + <delta>
         * WHERE  b_id = <b_id rnd>;
         *
         * plan: index probe on "B_IDX"
         */

        // 1. Probe Branches
        TRACE( TRACE_TRX_FLOW, 
               "App: %d UA:b-idx-nl (%d)\n", _tid.get_lo(), _in.b_id);
        e = _penv->branch_man()->b_idx_nl(_penv->db(), prb, _in.b_id);
        if (e.is_error()) { goto done; }

        double total;
        prb->get_value(1, total);
        prb->set_value(1, total + _in.delta);

        // 2. Update tuple
        e = _penv->branch_man()->update_tuple(_penv->db(), prb, NL);

    } // goto

#ifdef PRINT_TRX_RESULTS
    // dumps the status of all the table rows used
    prb->print_tuple();
#endif

done:
    // give back the tuple
    _penv->branch_man()->give_tuple(prb);
    return (e);
}



void upd_te_action::calc_keys()
{
    _down.push_back(_in.t_id);
}



w_rc_t upd_te_action::trx_exec() 
{
    assert (_penv);
    w_rc_t e = RCOK;

    // get table tuple from the cache
    // Subscriber
    table_row_t* prt = _penv->teller_man()->get_tuple();
    assert (prt);

    rep_row_t areprow(_penv->teller_man()->ts());
    areprow.set(_penv->teller_desc()->maxsize()); 
    prt->_rep = &areprow;


    { // make gotos safe

        /* UPDATE Tellers
         * SET    t_balance = t_balance + <delta>
         * WHERE  t_id = <t_id rnd>;
         *
         * plan: index probe on "T_IDX"
         */

        // 1. Probe Tellers
        TRACE( TRACE_TRX_FLOW, 
               "App: %d UA:t-idx-nl (%d)\n", _tid.get_lo(), _in.t_id);
        e = _penv->teller_man()->t_idx_nl(_penv->db(), prt, _in.t_id);
        if (e.is_error()) { goto done; }

        double total;
        prt->get_value(2, total);
        prt->set_value(2, total + _in.delta);

        // 2. Update tuple
        e = _penv->teller_man()->update_tuple(_penv->db(), prt, NL);

    } // goto

#ifdef PRINT_TRX_RESULTS
    // dumps the status of all the table rows used
    prt->print_tuple();
#endif

done:
    // give back the tuple
    _penv->teller_man()->give_tuple(prt);
    return (e);
}



void upd_ac_action::calc_keys()
{
    _down.push_back(_in.a_id);
}



w_rc_t upd_ac_action::trx_exec() 
{
    assert (_penv);
    w_rc_t e = RCOK;

    // get table tuple from the cache
    // Subscriber
    table_row_t* pra = _penv->account_man()->get_tuple();
    assert (pra);

    rep_row_t areprow(_penv->account_man()->ts());
    areprow.set(_penv->account_desc()->maxsize()); 
    pra->_rep = &areprow;


    { // make gotos safe

        /* UPDATE Accounts
         * SET    a_balance = a_balance + <delta>
         * WHERE  a_id = <a_id rnd>;
         *
         * plan: index probe on "A_IDX"
         */

        // 1. Probe Accounts
        TRACE( TRACE_TRX_FLOW, 
               "App: %d UA:a-idx-nl (%d)\n", _tid.get_lo(), _in.a_id);
        e = _penv->account_man()->a_idx_nl(_penv->db(), pra, _in.a_id);
        if (e.is_error()) { goto done; }

        double total;
        pra->get_value(2, total);
        pra->set_value(2, total + _in.delta);

        // 2. Update tuple
        e = _penv->account_man()->update_tuple(_penv->db(), pra, NL);

    } // goto

#ifdef PRINT_TRX_RESULTS
    // dumps the status of all the table rows used
    pra->print_tuple();
#endif

done:
    // give back the tuple
    _penv->account_man()->give_tuple(pra);
    return (e);
}



void ins_hi_action::calc_keys()
{
    _down.push_back(_in.a_id);
}



w_rc_t ins_hi_action::trx_exec() 
{
    assert (_penv);
    w_rc_t e = RCOK;

    // get table tuple from the cache
    // Subscriber
    table_row_t* prh = _penv->history_man()->get_tuple();
    assert (prh);

    rep_row_t areprow(_penv->account_man()->ts());
    areprow.set(_penv->account_desc()->maxsize()); 
    prh->_rep = &areprow;


    { // make gotos safe

        /* INSERT INTO History
         * VALUES (<t_id>, <b_id>, <a_id>, <delta>, <timestamp>)
         */

        // 1. Insert tuple

	prh->set_value(0, _in.b_id);
	prh->set_value(1, _in.t_id);
	prh->set_value(2, _in.a_id);
	prh->set_value(3, _in.delta);
	prh->set_value(4, time(NULL));

#ifdef CFG_HACK
	prh->set_value(5, "padding"); // PADDING
#endif

        TRACE( TRACE_TRX_FLOW, 
               "App: %d UA:ins-hi\n", _tid.get_lo());
        e = _penv->history_man()->add_tuple(_penv->db(), prh, NL);
        if (e.is_error()) { goto done; }

    } // goto

#ifdef PRINT_TRX_RESULTS
    // dumps the status of all the table rows used
    prh->print_tuple();
#endif

done:
    // give back the tuple
    _penv->history_man()->give_tuple(prh);
    return (e);
}




EXIT_NAMESPACE(dora);
