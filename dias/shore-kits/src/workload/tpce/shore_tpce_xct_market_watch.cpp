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

/** @file:   shore_tpce_xct_market_watch.cpp
 *
 *  @brief:  Implementation of the Baseline Shore TPC-E MARKET WATCH transaction
 *
 *  @author: Cansu Kaynak
 *  @author: Djordje Jevdjic
 */

#include "workload/tpce/shore_tpce_env.h"
#include "workload/tpce/tpce_const.h"
#include "workload/tpce/tpce_input.h"

#include <vector>
#include <numeric>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include "workload/tpce/egen/CE.h"
#include "workload/tpce/egen/TxnHarnessStructs.h"
#include "workload/tpce/shore_tpce_egen.h"

using namespace shore;
using namespace TPCE;

ENTER_NAMESPACE(tpce);

/******************************************************************** 
 *
 * TPC-E MARKET WATCH
 *
 ********************************************************************/

w_rc_t ShoreTPCEEnv::xct_market_watch(const int xct_id, market_watch_input_t& pmwin)
{
    // ensure a valid environment
    assert (_pssm);
    assert (_initialized);
    assert (_loaded);

    table_row_t* prcompany = _pcompany_man->get_tuple();
    assert (prcompany);

    table_row_t* prdailymarket = _pdaily_market_man->get_tuple();
    assert (prdailymarket);

    table_row_t* prholdsumm = _pholding_summary_man->get_tuple();
    assert (prholdsumm);

    table_row_t* prindustry = _pindustry_man->get_tuple();
    assert (prindustry);

    table_row_t* prlasttrade = _plast_trade_man->get_tuple();
    assert (prlasttrade);

    table_row_t* prsecurity = _psecurity_man->get_tuple();
    assert (prsecurity);

    table_row_t* prwatchitem = _pwatch_item_man->get_tuple();
    assert (prwatchitem);

    table_row_t* prwatchlist = _pwatch_list_man->get_tuple();
    assert (prwatchlist);

    w_rc_t e = RCOK;
    rep_row_t areprow(_pcompany_man->ts());
    areprow.set(_pcompany_desc->maxsize());

    prcompany->_rep = &areprow;
    prdailymarket->_rep = &areprow;
    prholdsumm->_rep = &areprow;
    prindustry->_rep = &areprow;
    prlasttrade->_rep = &areprow;
    prsecurity->_rep = &areprow;
    prwatchitem->_rep = &areprow;
    prwatchlist->_rep = &areprow;

    rep_row_t lowrep( _pcompany_man->ts());
    rep_row_t highrep( _pcompany_man->ts());

    // allocate space for the biggest of the table representations
    lowrep.set(_pcompany_desc->maxsize());
    highrep.set(_pcompany_desc->maxsize());

    {
	if(! ((pmwin._acct_id != 0) || (pmwin._cust_id != 0) || strcmp(pmwin._industry_name, "") != 0))
	    assert(false); //Harness control

	vector<string> stock_list;
	if(pmwin._cust_id != 0){
	    /**
	       select
	       WI_S_SYMB
	       from
	       WATCH_ITEM,
	       WATCH_LIST
	       where
	       WI_WL_ID = WL_ID and    //there are many watch_items in a watch_list
	       WL_C_ID = cust_id       //every customer has one watch_list
	    */

	    bool eof;
	    guard< index_scan_iter_impl<watch_list_t> > wl_iter;
	    {
		index_scan_iter_impl<watch_list_t>* tmp_wl_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d MW:wl-get-iter-by-idx2 (%ld) \n", xct_id, pmwin._cust_id);
		e = _pwatch_list_man->wl_get_iter_by_index2(_pssm, tmp_wl_iter, prwatchlist,
							    lowrep, highrep, pmwin._cust_id);
		if (e.is_error()) { goto done; }
		wl_iter = tmp_wl_iter;
	    }

	    TRACE( TRACE_TRX_FLOW, "App: %d MW:wl-iter-next \n", xct_id);
	    e = wl_iter->next(_pssm, eof, *prwatchlist);
	    if (e.is_error() || eof) { goto done; }

	    TIdent wl_id;
	    prwatchlist->get_value(0, wl_id);
	    
	    guard< index_scan_iter_impl<watch_item_t> > wi_iter;
	    {
		index_scan_iter_impl<watch_item_t>* tmp_wi_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d MW:wi-get-iter-by-idx (%ld) \n", xct_id,  wl_id);
		e = _pwatch_item_man->wi_get_iter_by_index(_pssm, tmp_wi_iter, prwatchitem, lowrep, highrep, wl_id);
		if (e.is_error()) { goto done; }
		wi_iter = tmp_wi_iter;
	    }

	    TRACE( TRACE_TRX_FLOW, "App: %d MW:wi-iter-next \n", xct_id);
	    e = wi_iter->next(_pssm, eof, *prwatchitem);
	    if (e.is_error()) { goto done; }
	    while(!eof){
		char wi_s_symb[16]; //15
		prwatchitem->get_value(1, wi_s_symb, 16);

		string symb(wi_s_symb);
		stock_list.push_back(symb);

		TRACE( TRACE_TRX_FLOW, "App: %d MW:wi-iter-next %s \n", xct_id, wi_s_symb);
		e = wi_iter->next(_pssm, eof, *prwatchitem);
		if (e.is_error()) { goto done; }
	    }
	}
	else if(pmwin._industry_name[0] != 0){
	    /**
	       select
	       S_SYMB
	       from
	       INDUSTRY, COMPANY, SECURITY
	       where
	       IN_NAME = industry_name and
	       CO_IN_ID = IN_ID and
	       CO_ID between (starting_co_id and ending_co_id) and
	       S_CO_ID = CO_ID
	    */

	    guard< index_scan_iter_impl<industry_t> > in_iter;
	    {
		index_scan_iter_impl<industry_t>* tmp_in_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d MW:in-get-iter-by-idx2 (%s) \n", xct_id, pmwin._industry_name);
		e = _pindustry_man->in_get_iter_by_index2(_pssm, tmp_in_iter, prindustry,
							  lowrep, highrep,  pmwin._industry_name);
		if (e.is_error()) { goto done; }
		in_iter = tmp_in_iter;
	    }
	    bool eof;
	    TRACE( TRACE_TRX_FLOW, "App: %d MW:in-iter-next \n", xct_id);
	    e = in_iter->next(_pssm, eof, *prindustry);
	    if (e.is_error() || eof) { goto done; }

	    char in_id[3];
	    prindustry->get_value(0, in_id, 3);
	    
	    guard< index_scan_iter_impl<company_t> > co_iter;
	    {
		index_scan_iter_impl<company_t>* tmp_co_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d MW:co-get-iter-by-idx3 (%s) \n", xct_id, in_id);
		e = _pcompany_man->co_get_iter_by_index3(_pssm, tmp_co_iter, prcompany, lowrep, highrep, in_id);
		co_iter = tmp_co_iter;
		if (e.is_error()) { goto done; }
	    }

	    TRACE( TRACE_TRX_FLOW, "App: %d MW:co-iter-next \n", xct_id);
	    e = co_iter->next(_pssm, eof, *prcompany);
	    if (e.is_error()) { goto done; }
	    TIdent co_id;

	    while(!eof){
		prcompany->get_value(0, co_id);

		if(pmwin._starting_co_id <= co_id && co_id <= pmwin._ending_co_id){
		    guard< index_scan_iter_impl<security_t> > s_iter;
		    {
			index_scan_iter_impl<security_t>* tmp_s_iter;
			TRACE( TRACE_TRX_FLOW, "App: %d MW:s-get-iter-by-idx4 (%ld) \n", xct_id, co_id);
			e = _psecurity_man->s_get_iter_by_index4(_pssm, tmp_s_iter, prsecurity,
								 lowrep, highrep, co_id);
			s_iter = tmp_s_iter;
			if (e.is_error()) { goto done; }
		    }

		    TRACE( TRACE_TRX_FLOW, "App: %d MW:s-iter-next \n", xct_id);
		    e = s_iter->next(_pssm, eof, *prsecurity);
		    if (e.is_error()) { goto done; }

		    while(!eof){				 
			char s_symb[16]; //15
			prsecurity->get_value(0, s_symb, 16);
			
			string symb(s_symb);
			stock_list.push_back(symb);
			
			TRACE( TRACE_TRX_FLOW, "App: %d TO:s-iter-next \n", xct_id);
			e = s_iter->next(_pssm, eof, *prsecurity);
			if (e.is_error()) { goto done; }
		    }
		}

		TRACE( TRACE_TRX_FLOW, "App: %d MW:co-iter-next \n", xct_id);
		e = co_iter->next(_pssm, eof, *prcompany);
		if (e.is_error()) { goto done; }
	    }
	}
	else if(pmwin._acct_id != 0){
	    /**
	       select
	       HS_S_SYMB
	       from
	       HOLDING_SUMMARY
	       where
	       HS_CA_ID = acct_id
	    */

	    guard< index_scan_iter_impl<holding_summary_t> > hs_iter;
	    {
		index_scan_iter_impl<holding_summary_t>* tmp_hs_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d MW:hs-iter-by-idx (%ld) \n", xct_id, pmwin._acct_id);
		e = _pholding_summary_man->hs_get_iter_by_index(_pssm, tmp_hs_iter, prholdsumm,
								lowrep, highrep, pmwin._acct_id, false);
		hs_iter = tmp_hs_iter;
		if (e.is_error()) { goto done; }
	    }

	    bool eof;
	    TRACE( TRACE_TRX_FLOW, "App: %d MW:hs-iter-next \n", xct_id);
	    e = hs_iter->next(_pssm, eof, *prholdsumm);
	    if (e.is_error()) { goto done; }
	    while(!eof){
		char hs_s_symb[16]; //15
		prholdsumm->get_value(1, hs_s_symb, 16);

		string symb(hs_s_symb);
		stock_list.push_back(symb);

		TRACE( TRACE_TRX_FLOW, "App: %d MW:hs-iter-next \n", xct_id);
		e = hs_iter->next(_pssm, eof, *prholdsumm);
		if (e.is_error()) { goto done; }
	    }
	}

	double old_mkt_cap = 0;
	double new_mkt_cap = 0;
	double pct_change;

	for(vector<string>::iterator stock_list_iter = stock_list.begin();
	    stock_list_iter != stock_list.end();
	    stock_list_iter++) {
	    
	    char symbol[16]; //15
	    strcpy(symbol, (*stock_list_iter).c_str());

	    /**
	       select
	       new_price = LT_PRICE
	       from
	       LAST_TRADE
	       where
	       LT_S_SYMB = symbol
	    */

	    TRACE( TRACE_TRX_FLOW, "App: %d MW:lt-idx-probe (%s) \n", xct_id, symbol);
	    e =  _plast_trade_man->lt_index_probe(_pssm, prlasttrade, symbol);
	    if(e.is_error()) { goto done; }

	    double new_price;
	    prlasttrade->get_value(2, new_price);


	    /**
	       select
	       s_num_out = S_NUM_OUT
	       from
	       SECURITY
	       where
	       S_SYMB = symbol
	    */
	    TRACE( TRACE_TRX_FLOW, "App: %d MW:s-idx-probe (%s) \n", xct_id, symbol);
	    e =  _psecurity_man->s_index_probe(_pssm, prsecurity, symbol);
	    if(e.is_error()) { goto done; }

	    double s_num_out;
	    prsecurity->get_value(6, s_num_out);

	    /**
	       select
	       old_price = DM_CLOSE
	       from
	       DAILY_MARKET
	       where
	       DM_S_SYMB = symbol and
	       DM_DATE = start_date
	    */

	    TRACE( TRACE_TRX_FLOW, "App: %d MW:dm-idx1-probe (%s) (%d) \n", xct_id, symbol, pmwin._start_date);
	    e =  _pdaily_market_man->dm_index_probe(_pssm, prdailymarket, symbol, pmwin._start_date);
	    if(e.is_error()) { goto done; }

	    double old_price;
	    prdailymarket->get_value(2, old_price);

	    old_mkt_cap += (s_num_out * old_price);
	    new_mkt_cap += (s_num_out * new_price);
	}

	if(old_mkt_cap != 0){
	    pct_change = 100 * (new_mkt_cap / old_mkt_cap - 1);
	}
	else{
	    pct_change = 0;
	}
    }

#ifdef PRINT_TRX_RESULTS
    // at the end of the transaction
    // dumps the status of all the table rows used
    rcompany.print_tuple();
    rdailymarket.print_tuple();
    rholdsumm.print_tuple();
    rindustry.print_tuple();
    rlasttrade.print_tuple();
    rsecurity.print_tuple();
    rwatchitem.print_tuple();
    rwatchlist.print_tuple();
#endif

 done:
    // return the tuples to the cache
    _pcompany_man->give_tuple(prcompany);
    _pdaily_market_man->give_tuple(prdailymarket);
    _pholding_summary_man->give_tuple(prholdsumm);
    _pindustry_man->give_tuple(prindustry);
    _plast_trade_man->give_tuple(prlasttrade);
    _psecurity_man->give_tuple(prsecurity);
    _pwatch_item_man->give_tuple(prwatchitem);
    _pwatch_list_man->give_tuple(prwatchlist);

    return (e);
}

EXIT_NAMESPACE(tpce);    
