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

/** @file:   shore_tpce_xct_data_maintenance.cpp
 *
 *  @brief:  Implementation of the Baseline Shore TPC-E DATA MAINTENANCE transaction
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
 * TPC-E DATA MAINTENANCE
 *
 ********************************************************************/

w_rc_t ShoreTPCEEnv::xct_data_maintenance(const int xct_id, data_maintenance_input_t& pdmin)
{
    // ensure a valid environment
    assert (_pssm);
    assert (_initialized);
    assert (_loaded);

    table_row_t* pracctperm = _paccount_permission_man->get_tuple();
    assert (pracctperm);

    table_row_t* praddress = _paddress_man->get_tuple();
    assert (praddress);

    table_row_t* prcompany = _pcompany_man->get_tuple();
    assert (prcompany);

    table_row_t* prcustomer = _pcustomer_man->get_tuple();
    assert (prcustomer);

    table_row_t* prcustomertaxrate = _pcustomer_taxrate_man->get_tuple();
    assert (prcustomertaxrate);

    table_row_t* prdailymarket = _pdaily_market_man->get_tuple();
    assert (prdailymarket);

    table_row_t* prexchange = _pexchange_man->get_tuple();
    assert (prexchange);

    table_row_t* prfinancial = _pfinancial_man->get_tuple();
    assert (prfinancial);

    table_row_t* prsecurity = _psecurity_man->get_tuple();
    assert (prsecurity);

    table_row_t* prnewsitem = _pnews_item_man->get_tuple();
    assert (prnewsitem);

    table_row_t* prnewsxref= _pnews_xref_man->get_tuple();
    assert (prnewsxref);

    table_row_t* prtaxrate = _ptaxrate_man->get_tuple();
    assert (prtaxrate);

    table_row_t* prwatchitem = _pwatch_item_man->get_tuple();
    assert (prwatchitem);

    table_row_t* prwatchlist = _pwatch_list_man->get_tuple();
    assert (prwatchlist);

    w_rc_t e = RCOK;
    rep_row_t areprow(_pnews_item_man->ts());
    areprow.set(_pnews_item_desc->maxsize());

    pracctperm->_rep = &areprow;
    praddress->_rep = &areprow;
    prcompany->_rep = &areprow;
    prcustomer->_rep = &areprow;
    prcustomertaxrate->_rep = &areprow;
    prdailymarket->_rep = &areprow;
    prexchange->_rep = &areprow;
    prfinancial->_rep = &areprow;
    prsecurity->_rep = &areprow;
    prnewsitem->_rep = &areprow;
    prnewsxref->_rep = &areprow;
    prtaxrate->_rep = &areprow;
    prwatchitem->_rep = &areprow;
    prwatchlist->_rep = &areprow;

    rep_row_t lowrep( _pnews_item_man->ts());
    rep_row_t highrep( _pnews_item_man->ts());

    // allocate space for the biggest of the table representations
    lowrep.set(_pnews_item_desc->maxsize());
    highrep.set(_pnews_item_desc->maxsize());
    {
	if(strcmp(pdmin._table_name, "ACCOUNT_PERMISSION") == 0){
	    /**
	       select first 1 row
	       acl = AP_ACL
	       from
	       ACCOUNT_PERMISSION
	       where
	       AP_CA_ID = acct_id
	       order by
	       AP_ACL DESC
	    */
	    guard<index_scan_iter_impl<account_permission_t> > ap_iter;
	    {
		index_scan_iter_impl<account_permission_t>* tmp_ap_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d DM:ap-get-iter-by-idx %ld \n", xct_id, pdmin._acct_id);
		e = _paccount_permission_man->ap_get_iter_by_index(_pssm, tmp_ap_iter,
								   pracctperm, lowrep, highrep,
								   pdmin._acct_id);
		if (e.is_error()) { goto done; }
		ap_iter = tmp_ap_iter;
	    }
	    //descending order
	    rep_row_t sortrep(_pnews_item_man->ts());
	    sortrep.set(_pnews_item_desc->maxsize());

	    desc_sort_buffer_t ap_list(1);

	    ap_list.setup(0, SQL_FIXCHAR, 4);

	    table_row_t rsb(&ap_list);
	    desc_sort_man_impl ap_sorter(&ap_list, &sortrep);

	    bool eof;
	    TRACE( TRACE_TRX_FLOW, "App: %d DM:ap-iter-next \n", xct_id);
	    e = ap_iter->next(_pssm, eof, *pracctperm);
	    if (e.is_error()) {  goto done; }

	    if(eof) {
		TRACE( TRACE_TRX_FLOW, "App: %d DM: no account permission tuple with acct_id (%d) \n",
		       xct_id, pdmin._acct_id);
		e = RC(se_NOT_FOUND);
		goto done;		 
	    }
		    
	    while(!eof){
		char acl[5]; //4
		pracctperm->get_value(1, acl, 5);

		rsb.set_value(0, acl);
		ap_sorter.add_tuple(rsb);

		TRACE( TRACE_TRX_FLOW, "App: %d DM:ap-iter-next \n", xct_id);
		e = ap_iter->next(_pssm, eof, *pracctperm);
		if (e.is_error()) {  goto done; }
	    }

	    desc_sort_iter_impl ap_list_sort_iter(_pssm, &ap_list, &ap_sorter);

	    TRACE( TRACE_TRX_FLOW, "App: %d DM:ap-sort-iter-next \n", xct_id);
	    e = ap_list_sort_iter.next(_pssm, eof, rsb);
	    if (e.is_error()) {  goto done; }

	    char acl[5]; //4
	    rsb.get_value(0, acl, 5);

	    char new_acl[5]; //4
	    if(strcmp(acl, "1111") != 0){
		/**
		   update
		   ACCOUNT_PERMISSION
		   set
		   AP_ACL="0011"
		   where
		   AP_CA_ID = acct_id and
		   AP_ACL = acl
		*/
		strcmp(new_acl, "1111");
	    }
	    else{
		/**
		   update
		   ACCOUNT_PERMISSION
		   set
		   AP_ACL = ”0011”
		   where
		   AP_CA_ID = acct_id and
		   AP_ACL = acl
		*/
		strcmp(new_acl, "0011");
	    }
	    {
		index_scan_iter_impl<account_permission_t>* tmp_ap_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d DM:ap-get-iter-by-idx %ld \n", xct_id, pdmin._acct_id);
		e = _paccount_permission_man->ap_get_iter_by_index(_pssm, tmp_ap_iter,
								   pracctperm, lowrep, highrep,
								   pdmin._acct_id);
		if (e.is_error()) {   goto done; }
		ap_iter = tmp_ap_iter;
	    }
	    
	    TRACE( TRACE_TRX_FLOW, "App: %d DM:ap-iter-next \n", xct_id);
	    e = ap_iter->next(_pssm, eof, *pracctperm);
	    if (e.is_error()) {  goto done; }
	    while(!eof){
		char ap_acl[5]; //4
		pracctperm->get_value(1, ap_acl, 5);
		
		if(strcmp(ap_acl, acl) == 0){
		    TRACE( TRACE_TRX_FLOW, "App: %d DM:ap-update (%s) \n", xct_id, new_acl);
		    e = _paccount_permission_man->ap_update_acl(_pssm, pracctperm, new_acl);
		    if (e.is_error()) {  goto done; }
		}
		
		TRACE( TRACE_TRX_FLOW, "App: %d DM:ap-iter-next \n", xct_id);
		e = ap_iter->next(_pssm, eof, *pracctperm);
		if (e.is_error()) {  goto done; }
	    }
	    
	} else if(strcmp(pdmin._table_name, "ADDRESS") == 0){
	    char line2[81] = "\0"; //80
	    TIdent ad_id = 0;

	    if(pdmin._c_id != 0){
		/**
		   select
		   line2 = AD_LINE2,
		   ad_id = AD_ID
		   from
		   ADDRESS, CUSTOMER
		   where
		   AD_ID = C_AD_ID and
		   C_ID = c_id
		*/

		TRACE( TRACE_TRX_FLOW, "App: %d DM:c-idx-probe (%ld) \n", xct_id,  pdmin._c_id);
		e =  _pcustomer_man->c_index_probe(_pssm, prcustomer, pdmin._c_id);
		if (e.is_error()) {  goto done; }
		prcustomer->get_value(9, ad_id);

	    } else {
		/**
		   select
		   line2 = AD_LINE2,
		   ad_id = AD_ID
		   from
		   ADDRESS, COMPANY
		   where
		   AD_ID = CO_AD_ID and
		   CO_ID = co_id
		*/

		TRACE( TRACE_TRX_FLOW, "App: %d DM:co-idx-probe (%ld) \n", xct_id,  pdmin._co_id);
		e =  _pcompany_man->co_index_probe(_pssm, prcompany, pdmin._co_id);
		if (e.is_error()) {  goto done; }
		prcompany->get_value(6, ad_id);
	    }
	    TRACE( TRACE_TRX_FLOW, "App: %d DM:ad-idx-probe-for-update (%ld) \n", xct_id,  ad_id);
	    e =  _paddress_man->ad_index_probe_forupdate(_pssm, praddress, ad_id);
	    if (e.is_error()) {  goto done; }	    
	    praddress->get_value(2, line2, 81);


	    char new_line2[81];
	    if(strcmp(line2, "Apt. 10C") != 0){
		/**
		   update
		   ADDRESS
		   set
		   AD_LINE2 = “Apt. 10C”
		   where
		   AD_ID = ad_id
		*/
		strcpy(new_line2, "Apt. 10C");
	    } else{
		/**
		   update
		   ADDRESS
		   set
		   AD_LINE2 = “Apt. 22”
		   where
		   AD_ID = ad_id
		*/
		strcpy(new_line2, "Apt. 22");
	    }
	    TRACE( TRACE_TRX_FLOW, "App: %d DM:ad-update (%ld) (%s) \n", xct_id, ad_id, new_line2);
	    e = _paddress_man->ad_update_line2(_pssm, praddress, new_line2);
	    if (e.is_error()) {  goto done; }

	} else if(strcmp(pdmin._table_name, "COMPANY") == 0){
	    char sprate[5] = "\0"; //4

	    TRACE( TRACE_TRX_FLOW, "App: %d DM:co-idx-probe (%ld) \n", xct_id,  pdmin._co_id);
	    e =  _pcompany_man->co_index_probe_forupdate(_pssm, prcompany, pdmin._co_id);
	    if(e.is_error()) {  goto done; }
	    prcompany->get_value(4, sprate, 5);

	    char new_sprate[5];
	    if(strcmp(sprate, "ABA") != 0){
		/**
		   update
		   COMPANY
		   set
		   CO_SP_RATE = “ABA”
		   where
		   CO_ID = co_id
		*/
		strcpy(new_sprate, "ABA");
	    } else {
		/**
		   update
		   COMPANY
		   set
		   CO_SP_RATE = “AAA”
		   where
		   CO_ID = co_id
		*/
		strcpy(new_sprate, "AAA");
	    }
	    TRACE( TRACE_TRX_FLOW, "App: %d DM:co-update (%s) \n", xct_id, new_sprate);
	    e = _pcompany_man->co_update_sprate(_pssm, prcompany, new_sprate);
	    if (e.is_error()) {  goto done; }

	} else if(strcmp(pdmin._table_name, "CUSTOMER") == 0){
	    char email2[51] = "\0"; //50
	    int len = 0;
	    int lenMindspring = strlen("@mindspring.com");

	    /**
	       select
	       email2 = C_EMAIL_2
	       from
	       CUSTOMER
	       where
	       C_ID = c_id
	    */

	    TRACE( TRACE_TRX_FLOW, "App: %d DM:c-idx-probe (%d) \n", xct_id,  pdmin._c_id);
	    e =  _pcustomer_man->c_index_probe_forupdate(_pssm, prcustomer, pdmin._c_id);
	    if (e.is_error()) {  goto done; }

	    prcustomer->get_value(23, email2, 51);

	    len = strlen(email2);
	    string temp_email2(email2);
	    char new_email2[51];
	    if( ((len - lenMindspring) > 0 &&
		 (temp_email2.substr(len-lenMindspring, lenMindspring).compare("@mindspring.com") == 0))){
		/**
		   update
		   CUSTOMER
		   set
		   C_EMAIL_2 = substring(C_EMAIL_2, 1,
		   charindex(“@”,C_EMAIL_2) ) + „earthlink.com‟
		   where
		   C_ID = c_id
		*/
		string temp_new_email2 = temp_email2.substr(1, temp_email2.find_first_of('@')) + "earthlink.com";
		strcpy(new_email2, temp_new_email2.c_str());
	    } else{
		/**
		   update
		   CUSTOMER
		   set
		   C_EMAIL_2 = substring(C_EMAIL_2, 1,
		   charindex(“@”,C_EMAIL_2) ) + „mindspring.com‟
		   where
		   C_ID = c_id
		*/
		string temp_new_email2 = temp_email2.substr(1, temp_email2.find_first_of('@')) + "mindspring.com";
		strcpy(new_email2, temp_new_email2.c_str());
	    }

	    TRACE( TRACE_TRX_FLOW, "App: %d DM:c-update (%s) \n", xct_id, new_email2);
	    e = _pcustomer_man->c_update_email2(_pssm, prcustomer, new_email2);
	    if (e.is_error()) {  goto done; }

	} else if(strcmp(pdmin._table_name, "CUSTOMER_TAXRATE") == 0){
	    char old_tax_rate[5];//4
	    char new_tax_rate[5];//4
	    int tax_num;

	    /**
	       select
	       old_tax_rate = CX_TX_ID
	       from
	       CUSTOMER_TAXRATE
	       where
	       CX_C_ID = c_id and (CX_TX_ID like “US%” or CX_TX_ID like “CN%”)
	    */
	    guard< index_scan_iter_impl<customer_taxrate_t> > cx_iter;
	    {
		index_scan_iter_impl<customer_taxrate_t>* tmp_cx_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d DM:cx-get-iter-by-idx (%ld) \n", xct_id, pdmin._c_id);
		e = _pcustomer_taxrate_man->cx_get_iter_by_index(_pssm, tmp_cx_iter, prcustomertaxrate,
								 lowrep, highrep, pdmin._c_id);
		if (e.is_error()) {  goto done; }
		cx_iter = tmp_cx_iter;
	    }

	    bool eof;
	    TRACE( TRACE_TRX_FLOW, "App: %d DM:cx-iter-next \n", xct_id);
	    e = cx_iter->next(_pssm, eof, *prcustomertaxrate);
	    if (e.is_error()) {  goto done; }
	    while(!eof){
		prcustomertaxrate->get_value(0, old_tax_rate, 5);

		string temp_old_tax_rate(old_tax_rate);
		if(temp_old_tax_rate.substr(0,2).compare("US") == 0 || temp_old_tax_rate.substr(0,2).compare("CN") == 0 ){

		    if(temp_old_tax_rate.substr(0,2).compare("US") == 0){
			if(temp_old_tax_rate.compare("US5") == 0){
			    strcpy(new_tax_rate, "US1");
			} else {
			    tax_num = atoi(temp_old_tax_rate.substr(2, 1).c_str()) + 1;
			    stringstream temp_new_tax_rate;
			    temp_new_tax_rate << "US" << tax_num;
			    strcpy(new_tax_rate, temp_new_tax_rate.str().c_str());
			}
		    } else {
			if(temp_old_tax_rate.compare("CN4") == 0){
			    strcpy(new_tax_rate, "CN1");
			} else {
			    tax_num = atoi(temp_old_tax_rate.substr(2, 1).c_str()) + 1;
			    stringstream temp_new_tax_rate;
			    temp_new_tax_rate << "CN" << tax_num;
			    strcpy(new_tax_rate, temp_new_tax_rate.str().c_str());
			}
		    }

		    /**
		       update
		       CUSTOMER_TAXRATE
		       set
		       CX_TX_ID = new_tax_rate
		       where
		       CX_C_ID = c_id and
		       CX_TX_ID = old_tax_rate
		    */

		    TRACE( TRACE_TRX_FLOW, "App: %d DM:cx-update (%s) \n", xct_id, new_tax_rate);
		    e = _pcustomer_taxrate_man->cx_update_txid(_pssm, prcustomertaxrate, new_tax_rate);
		    if (e.is_error()) {  goto done; }
		}
		TRACE( TRACE_TRX_FLOW, "App: %d DM:cx-iter-next \n", xct_id);
		e = cx_iter->next(_pssm, eof, *prcustomertaxrate);
		if (e.is_error()) {  goto done; }
	    }
	    
	} else if(strcmp(pdmin._table_name, "DAILY_MARKET") == 0){
	    /**
	       update
	       DAILY_MARKET
	       set
	       DM_VOL = DM_VOL + vol_incr
	       where
	       DM_S_SYMB = symbol
	       and substring ((convert(char(8),DM_DATE,3),1,2) = day_of_month
	    */

	    guard< index_scan_iter_impl<daily_market_t> > dm_iter;
	    {
		index_scan_iter_impl<daily_market_t>* tmp_dm_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d DM:dm-get-iter-by-idx4 (%s) \n", xct_id, pdmin._symbol);
		e = _pdaily_market_man->dm_get_iter_by_index(_pssm, tmp_dm_iter, prdailymarket,
							     lowrep, highrep, pdmin._symbol,
							     0, SH, false);
		if (e.is_error()) { goto done; }
		dm_iter = tmp_dm_iter;
	    }

	    bool eof;
	    e = dm_iter->next(_pssm, eof, *prdailymarket);
	    if (e.is_error()) { goto done; }
	    while(!eof){
		myTime dm_date;
		prdailymarket->get_value(0, dm_date);

		if(dayOfMonth(dm_date) == pdmin._day_of_month){
		    TRACE( TRACE_TRX_FLOW, "App: %d MD:dm-update (%d) \n", xct_id, pdmin._vol_incr);
		    e = _pdaily_market_man->dm_update_vol(_pssm, prdailymarket, pdmin._vol_incr);
		    if (e.is_error()) {  goto done; }
		}

		e = dm_iter->next(_pssm, eof, *prdailymarket);
		if (e.is_error()) { goto done; }
	    }
	    
	} else if(strcmp(pdmin._table_name, "EXCHANGE") == 0){
	    int rowcount = 0;
	    /**
	       select
	       rowcount = count(*)
	       from
	       EXCHANGE
	       where
	       EX_DESC like “%LAST UPDATED%”
	    */

	    guard<table_scan_iter_impl<exchange_t> > ex_iter;
	    {
		table_scan_iter_impl<exchange_t>* tmp_ex_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d DM:ex-get-table-iter \n", xct_id);
		e = _pexchange_man->get_iter_for_file_scan(_pssm, tmp_ex_iter);
		if (e.is_error()) {  goto done; }
		ex_iter = tmp_ex_iter;
	    }

	    bool eof;
	    e = ex_iter->next(_pssm, eof, *prexchange);
	    if (e.is_error()) { goto done; }
	    while(!eof){
		char ex_desc[151]; //150
		prexchange->get_value(5, ex_desc, 151);

		string temp_ex_desc(ex_desc);
		if(temp_ex_desc.find("LAST UPDATED") != -1){
		    rowcount++;
		}

		e = ex_iter->next(_pssm, eof, *prexchange);
		if (e.is_error()) {  goto done; }
	    }
	    
	    if(rowcount == 0){
		/**
		   update
		   EXCHANGE
		   set
		   EX_DESC = EX_DESC + “ LAST UPDATED “ + getdatetime()
		*/
		{
		    table_scan_iter_impl<exchange_t>* tmp_ex_iter;
		    TRACE( TRACE_TRX_FLOW, "App: %d DM:ex-get-table-iter \n", xct_id);
		    e = _pexchange_man->get_iter_for_file_scan(_pssm, tmp_ex_iter);
		    if (e.is_error()) {  goto done; }
		    ex_iter = tmp_ex_iter;
		}
		e = ex_iter->next(_pssm, eof, *prexchange);
		if (e.is_error()) {  goto done; }
		while(!eof){
		    char ex_desc[151]; //150
		    prexchange->get_value(5, ex_desc, 151);

		    string new_desc(ex_desc);
		    stringstream ss;
		    ss << "" << new_desc << " LAST UPDATED " << time(NULL) << "";

		    TRACE( TRACE_TRX_FLOW, "App: %d MD:ex-update (%s) \n", xct_id, ss.str().c_str());
		    e = _pexchange_man->ex_update_desc(_pssm, prexchange, ss.str().c_str());
		    if (e.is_error()) {  goto done; }

		    e = ex_iter->next(_pssm, eof, *prexchange);
		    if (e.is_error()) {  goto done; }
		}

	    } else {
		/**
		   update
		   EXCHANGE
		   set
		   EX_DESC = substring(EX_DESC,1,
		   len(EX_DESC)-len(getdatetime())) + getdatetime()
		*/
		table_scan_iter_impl<exchange_t>* tmp_ex_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d DM:ex-get-table-iter \n", xct_id);
		e = _pexchange_man->get_iter_for_file_scan(_pssm, tmp_ex_iter);
		if (e.is_error()) { goto done; }
		ex_iter = tmp_ex_iter;

		e = ex_iter->next(_pssm, eof, *prexchange);
		if (e.is_error()) {  goto done; }
		while(!eof){
		    ex_iter = tmp_ex_iter;
		    char ex_desc[151]; //150
		    prexchange->get_value(5, ex_desc, 151);

		    string temp(ex_desc), new_desc;
		    new_desc = temp.substr(0, temp.find_last_of(" ") + 1);
		    stringstream ss;
		    ss << "" << new_desc << time(NULL);

		    TRACE( TRACE_TRX_FLOW, "App: %d MD:ex-update (%s) \n", xct_id, ss.str().c_str());
		    e = _pexchange_man->ex_update_desc(_pssm, prexchange, ss.str().c_str());
		    if (e.is_error()) { goto done; }

		    e = ex_iter->next(_pssm, eof, *prexchange);
		    if (e.is_error()) { goto done; }
		}
	    }
	    
	} else if(strcmp(pdmin._table_name, "FINANCIAL") == 0){
	    int rowcount = 0;

	    /**
	       select
	       rowcount = count(*)
	       from
	       FINANCIAL
	       where
	       FI_CO_ID = co_id and
	       substring(convert(char(8),
	       FI_QTR_START_DATE,2),7,2) = “01”
	    */

	    guard< index_scan_iter_impl<financial_t> > fi_iter;
	    {
		index_scan_iter_impl<financial_t>* tmp_fi_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d DM:fi-get-iter-by-idx (%ld) \n", xct_id,  pdmin._co_id);
		e = _pfinancial_man->fi_get_iter_by_index(_pssm, tmp_fi_iter, prfinancial,
							  lowrep, highrep, pdmin._co_id);
		if (e.is_error()) {  goto done; }
		fi_iter = tmp_fi_iter;
	    }

	    bool eof;
	    TRACE( TRACE_TRX_FLOW, "App: %d DM:fi-iter-next \n", xct_id);
	    e = fi_iter->next(_pssm, eof, *prfinancial);
	    if (e.is_error()) {  goto done; }
	    while(!eof){
		myTime fi_qtr_start_date;
		prfinancial->get_value(3, fi_qtr_start_date);

		if(dayOfMonth(fi_qtr_start_date) == 1){
		    rowcount++;
		}

		TRACE( TRACE_TRX_FLOW, "App: %d DM:fi-iter-next \n", xct_id);
		e = fi_iter->next(_pssm, eof, *prfinancial);
		if (e.is_error()) {  goto done; }
	    }

	    if(rowcount > 0){
		/**
		   update
		   FINANCIAL
		   set
		   FI_QTR_START_DATE = FI_QTR_START_DATE + 1 day
		   where
		   FI_CO_ID = co_id
		*/
		{
		    index_scan_iter_impl<financial_t>* tmp_fi_iter;
		    TRACE( TRACE_TRX_FLOW, "App: %d DM:fi-get-iter-by-idx (%ld) \n", xct_id,  pdmin._co_id);
		    e = _pfinancial_man->fi_get_iter_by_index(_pssm, tmp_fi_iter, prfinancial,
							      lowrep, highrep, pdmin._co_id);
		    if (e.is_error()) {  goto done; }
		    fi_iter = tmp_fi_iter;
		}
		TRACE( TRACE_TRX_FLOW, "App: %d DM:fi-iter-next \n", xct_id);
		e = fi_iter->next(_pssm, eof, *prfinancial);
		if (e.is_error()) { goto done; }
		while(!eof){
		    myTime fi_qtr_start_date;
		    prfinancial->get_value(3, fi_qtr_start_date);

		    fi_qtr_start_date += (60*60*24); // add 1 day

		    TRACE( TRACE_TRX_FLOW, "App: %d MD:fi-update (%ld) \n", xct_id, fi_qtr_start_date);
		    e = _pfinancial_man->fi_update_desc(_pssm, prfinancial, fi_qtr_start_date);
		    if (e.is_error()) {  goto done; }

		    TRACE( TRACE_TRX_FLOW, "App: %d DM:fi-iter-next \n", xct_id);
		    e = fi_iter->next(_pssm, eof, *prfinancial);
		    if (e.is_error()) {  goto done; }
		}
	    } else {
		/**
		   update
		   FINANCIAL
		   set
		   FI_QTR_START_DATE = FI_QTR_START_DATE – 1 day
		   where
		   FI_CO_ID = co_id
		*/
		{
		    index_scan_iter_impl<financial_t>* tmp_fi_iter;
		    TRACE( TRACE_TRX_FLOW, "App: %d DM:fi-get-iter-by-idx (%ld) \n", xct_id,  pdmin._co_id);
		    e = _pfinancial_man->fi_get_iter_by_index(_pssm, tmp_fi_iter, prfinancial,
							      lowrep, highrep, pdmin._co_id);
		    if (e.is_error()) { goto done; }
		    fi_iter = tmp_fi_iter;
		}
		TRACE( TRACE_TRX_FLOW, "App: %d DM:fi-iter-next \n", xct_id);
		e = fi_iter->next(_pssm, eof, *prfinancial);
		if (e.is_error()) {  goto done; }
		while(!eof){
		    myTime fi_qtr_start_date;
		    prfinancial->get_value(3, fi_qtr_start_date);

		    fi_qtr_start_date -= (60*60*24);

		    TRACE( TRACE_TRX_FLOW, "App: %d MD:fi-update-desc (%ld) \n", xct_id, fi_qtr_start_date);
		    e = _pfinancial_man->fi_update_desc(_pssm, prfinancial, fi_qtr_start_date);
		    if (e.is_error()) {  goto done; }

		    TRACE( TRACE_TRX_FLOW, "App: %d DM:fi-iter-next \n", xct_id);
		    e = fi_iter->next(_pssm, eof, *prfinancial);
		    if (e.is_error()) {  goto done; }
		}
	    }
	    
	} else if(strcmp(pdmin._table_name, "NEWS_ITEM") == 0){
	    /**
	       update
	       NEWS_ITEM
	       set
	       NI_DTS = NI_DTS + 1day
	       where
	       NI_ID = (
	       select
	       NX_NI_ID
	       from
	       NEWS_XREF
	       where
	       NX_CO_ID = @co_id)
	    */
	    guard< index_scan_iter_impl<news_xref_t> > nx_iter;
	    {
		index_scan_iter_impl<news_xref_t>* tmp_nx_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d DM:nx-get-iter-by-idx (%ld) \n", xct_id, pdmin._co_id);
		e = _pnews_xref_man->nx_get_iter_by_index(_pssm, tmp_nx_iter, prnewsxref,
							  lowrep, highrep, pdmin._co_id);
		if (e.is_error()) {  goto done; }
		nx_iter = tmp_nx_iter;
	    }

	    bool eof;
	    e = nx_iter->next(_pssm, eof, *prnewsxref);
	    if (e.is_error()) {  goto done; }
	    while(!eof){
		TIdent nx_ni_id;
		prnewsxref->get_value(0, nx_ni_id);

		TRACE( TRACE_TRX_FLOW, "App: %d DM:nx-idx-probe (%d) \n", xct_id, nx_ni_id);
		e =  _pnews_item_man->ni_index_probe_forupdate(_pssm, prnewsitem, nx_ni_id);
		if (e.is_error()) {  goto done; }

		myTime ni_dts;
		prnewsitem->get_value(4, ni_dts);

		ni_dts += (60 * 60 * 24);

		TRACE( TRACE_TRX_FLOW, "App: %d DM:nx-update-nidts (%d) \n", xct_id, ni_dts);
		e =  _pnews_item_man->ni_update_dts_by_index(_pssm, prnewsitem, ni_dts);
		if (e.is_error()) {  goto done; }

		e = nx_iter->next(_pssm, eof, *prnewsxref);
		if (e.is_error()) {  goto done; }
	    }
	    
	} else if(strcmp(pdmin._table_name, "SECURITY") == 0){
	    /**
	       update
	       SECURITY
	       set
	       S_EXCH_DATE = S_EXCH_DATE + 1day
	       where
	       S_SYMB = symbol
	    */

	    TRACE( TRACE_TRX_FLOW, "App: %d DM:s-idx-probe (%s) \n", xct_id, pdmin._symbol);
	    e =  _psecurity_man->s_index_probe_forupdate(_pssm, prsecurity, pdmin._symbol);
	    if (e.is_error()) {  goto done; }

	    myTime s_exch_date;
	    prsecurity->get_value(8, s_exch_date);

	    s_exch_date += (60 * 60 * 24);

	    TRACE( TRACE_TRX_FLOW, "App: %d DM:s-update-ed (%d) \n", xct_id, s_exch_date );
	    e =  _psecurity_man->s_update_ed(_pssm, prsecurity, s_exch_date);
	    if (e.is_error()) {  goto done; }

	} else if(strcmp(pdmin._table_name, "TAXRATE") == 0){
	    /**
	       select
	       tx_name = TX_NAME
	       from
	       TAXRATE
	       where
	       TX_ID = tx_id	
	    */
	    
	    char tx_name[51]; //50
	    
	    TRACE( TRACE_TRX_FLOW, "App: %d DM:tx-idx-probe (%d) \n", xct_id, pdmin._tx_id);
	    e =  _ptaxrate_man->tx_index_probe_forupdate(_pssm, prtaxrate, pdmin._tx_id);
	    if (e.is_error()) {  goto done; }

	    prtaxrate->get_value(1, tx_name, 51);

	    string temp(tx_name);
	    size_t index = temp.find("Tax");
	    if(index != string::npos){
		temp.replace(index, 3, "tax");
	    } else {
		index = temp.find("tax");
		temp.replace(index, 3, "Tax");
	    }

	    /**
	       update
	       TAXRATE
	       set
	       TX_NAME = tx_name
	       where
	       TX_ID = tx_id
	    */
	    
	    TRACE( TRACE_TRX_FLOW, "App: %d DM:tx-update-name (%s) \n", xct_id, tx_name);
	    e =  _ptaxrate_man->tx_update_name(_pssm, prtaxrate, temp.c_str());
	    if (e.is_error()) {  goto done; }
	    
	} else if(strcmp(pdmin._table_name, "WATCH_ITEM") == 0){
	    // PIN: TODO: this part can be optimized, the same stuff is scanned bizillion times
	    
	    /**
	       select
	       cnt = count(*)     // number of rows is [50..150]
	       from
	       WATCH_ITEM,
	       WATCH_LIST
	       where
	       WL_C_ID = c_id and
	       WI_WL_ID = WL_ID
	    */

	    int cnt = 0;
	    char old_symbol[16], new_symbol[16] = "\0"; //15, 15

	    guard< index_scan_iter_impl<watch_list_t> > wl_iter;
	    {
		index_scan_iter_impl<watch_list_t>* tmp_wl_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d DM:wl-get-iter-by-idx2 (%ld) \n", xct_id,  pdmin._c_id);
		e = _pwatch_list_man->wl_get_iter_by_index2(_pssm, tmp_wl_iter, prwatchlist,
							    lowrep, highrep, pdmin._c_id);
		if (e.is_error()) {  goto done; }
		wl_iter = tmp_wl_iter;
	    }

	    bool eof;
	    e = wl_iter->next(_pssm, eof, *prwatchlist);
	    if (e.is_error()) {  goto done; }
	    while(!eof){
		TIdent wl_id;
		prwatchlist->get_value(0, wl_id);

		guard< index_scan_iter_impl<watch_item_t> > wi_iter;
		{
		    index_scan_iter_impl<watch_item_t>* tmp_wi_iter;
		    TRACE( TRACE_TRX_FLOW, "App: %d DM:wi-get-iter-by-idx (%ld) \n", xct_id,  wl_id);
		    e = _pwatch_item_man->wi_get_iter_by_index(_pssm, tmp_wi_iter, prwatchitem,
							       lowrep, highrep, wl_id);
		    if (e.is_error()) {  goto done; }
		    wi_iter = tmp_wi_iter;
		}

		e = wi_iter->next(_pssm, eof, *prwatchitem);
		if (e.is_error()) {  goto done; }
		while(!eof){
		    cnt++;

		    char wi_s_symbol[16]; //15
		    prwatchitem->get_value(1, wi_s_symbol, 16);

		    e = wi_iter->next(_pssm, eof, *prwatchitem);
		    if (e.is_error()) {  goto done; }
		}

		e = wl_iter->next(_pssm, eof, *prwatchlist);
		if (e.is_error()) {  goto done; }
	    }

	    cnt = (cnt+1)/2;

	    /**
	       select
	       old_symbol = WI_S_SYMB
	       from
	       ( select
	       ROWNUM,
	       WI_S_SYMB
	       from
	       WATCH_ITEM,
	       WATCH_LIST
	       where
	       WL_C_ID = c_id and
	       WI_WL_ID = WL_ID and
	       order by
	       WI_S_SYMB asc
	       )
	       where
	       rownum = cnt
	    */
	    //already sorted in ascending order because of its index

	    index_scan_iter_impl<watch_list_t>* tmp_wl_iter;
	    TRACE( TRACE_TRX_FLOW, "App: %d DM:wl-get-iter-by-idx2 (%ld) \n", xct_id,  pdmin._c_id);
	    e = _pwatch_list_man->wl_get_iter_by_index2(_pssm, tmp_wl_iter, prwatchlist,
							lowrep, highrep, pdmin._c_id);
	    if (e.is_error()) {  goto done; }
	    wl_iter = tmp_wl_iter;

	    e = wl_iter->next(_pssm, eof, *prwatchlist);
	    if (e.is_error()) { goto done; }
	    while(!eof && cnt > 0){
		TIdent wl_id;
		prwatchlist->get_value(0, wl_id);

		guard< index_scan_iter_impl<watch_item_t> > wi_iter;
		{
		    index_scan_iter_impl<watch_item_t>* tmp_wi_iter;
		    TRACE( TRACE_TRX_FLOW, "App: %d DM:wi-get-iter-by-idx (%ld) \n", xct_id,  wl_id);
		    e = _pwatch_item_man->wi_get_iter_by_index(_pssm, tmp_wi_iter, prwatchitem,
							       lowrep, highrep, wl_id);
		    if (e.is_error()) {  goto done; }
		    wi_iter = tmp_wi_iter;
		}

		e = wi_iter->next(_pssm, eof, *prwatchitem);
		if (e.is_error()) {  goto done; }
		while(!eof){
		    cnt--;
		    if(cnt == 0){
			prwatchitem->get_value(1, old_symbol, 16);
			break;
		    }

		    e = wi_iter->next(_pssm, eof, *prwatchitem);
		    if (e.is_error()) {  goto done; }
		}

		e = wl_iter->next(_pssm, eof, *prwatchlist);
		if (e.is_error()) {  goto done; }
	    }

	    /**
	       select first 1
	       new_symbol = S_SYMB
	       from
	       SECURITY
	       where
	       S_SYMB > old_symbol and
	       S_SYMB not in (
	       select
	       WI_S_SYMB
	       from
	       WATCH_ITEM,
	       WATCH_LIST
	       where
	       WL_C_ID = c_id and
	       WI_WL_ID = WL_ID
	       )
	       order by
	       S_SYMB asc
	    */
	    //already sorted in ascending order because of its index

	    guard< index_scan_iter_impl<security_t> > s_iter;
	    {
		index_scan_iter_impl<security_t>* tmp_s_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d DM:s-get-iter-by-idx (%s) \n", xct_id, old_symbol);
		e = _psecurity_man->s_get_iter_by_index(_pssm, tmp_s_iter, prsecurity, lowrep, highrep, old_symbol);
		if (e.is_error()) {  goto done; }
		s_iter = tmp_s_iter;
	    }

	    e = s_iter->next(_pssm, eof, *prsecurity);
	    if (e.is_error()) {  goto done; }
	    while(!eof){
		char s_symb[16]; //15
		prsecurity->get_value(0, s_symb, 16);

		index_scan_iter_impl<watch_list_t>* tmp_wl_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d DM:wl-get-iter-by-idx2 (%ld) \n", xct_id,  pdmin._c_id);
		e = _pwatch_list_man->wl_get_iter_by_index2(_pssm, tmp_wl_iter, prwatchlist,
							    lowrep, highrep, pdmin._c_id);
		if (e.is_error()) {  goto done; }
		wl_iter = tmp_wl_iter;

		e = wl_iter->next(_pssm, eof, *prwatchlist);
		if (e.is_error()) {  goto done; }
		while(!eof){
		    TIdent wl_id;
		    prwatchlist->get_value(0, wl_id);

		    guard< index_scan_iter_impl<watch_item_t> > wi_iter;
		    {
			index_scan_iter_impl<watch_item_t>* tmp_wi_iter;
			TRACE( TRACE_TRX_FLOW, "App: %d DM:wi-get-iter-by-idx (%ld) \n", xct_id,  wl_id);
			e = _pwatch_item_man->wi_get_iter_by_index(_pssm, tmp_wi_iter, prwatchitem,
								   lowrep, highrep, wl_id);
			if (e.is_error()) {  goto done; }
			wi_iter = tmp_wi_iter;
		    }

		    e = wi_iter->next(_pssm, eof, *prwatchitem);
		    if (e.is_error()) {  goto done; }
		    while(!eof){
			char wi_s_symb[16]; //15
			prwatchitem->get_value(1, wi_s_symb, 16);

			if(strcmp(s_symb, wi_s_symb) != 0){
			    strcpy(new_symbol, s_symb);
			    break;
			}

			e = wi_iter->next(_pssm, eof, *prwatchitem);
			if (e.is_error()) {  goto done; }
		    }
		    if(strcmp(new_symbol, "\0") != 0)
			break;

		    e = wl_iter->next(_pssm, eof, *prwatchlist);
		    if (e.is_error()) {  goto done; }
		}
		if(strcmp(new_symbol, "\0") != 0)
		    break;

		e = s_iter->next(_pssm, eof, *prsecurity);
		if (e.is_error()) {  goto done; }
	    }

	    /**
	       update
	       WATCH_ITEM
	       set
	       WI_S_SYMB = new_symbol
	       from
	       WATCH_LIST
	       where
	       WL_C_ID = c_id and
	       WI_WL_ID = WL_ID and
	       WI_S_SYMB = old_symbol
	    */

	    TRACE( TRACE_TRX_FLOW, "App: %d DM:wl-get-iter-by-idx2 (%ld) \n", xct_id,  pdmin._c_id);
	    e = _pwatch_list_man->wl_get_iter_by_index2(_pssm, tmp_wl_iter, prwatchlist,
							lowrep, highrep, pdmin._c_id);
	    if (e.is_error()) {  goto done; }
	    wl_iter = tmp_wl_iter;

	    e = wl_iter->next(_pssm, eof, *prwatchlist);
	    if (e.is_error()) {  goto done; }
	    while(!eof && cnt > 0){
		TIdent wl_id;
		prwatchlist->get_value(0, wl_id);

		TRACE( TRACE_TRX_FLOW, "App: %d DM:wi-update-symb (%ld) (%s) (%s) \n",
		       xct_id, wl_id, old_symbol, new_symbol);
		e =  _pwatch_item_man->wi_update_symb(_pssm, prwatchitem, wl_id, old_symbol, new_symbol);
		if(e.is_error()) {  goto done; }

		e = wl_iter->next(_pssm, eof, *prwatchlist);
		if (e.is_error()) {  goto done; }
	    }
	}
    }

#ifdef PRINT_TRX_RESULTS
    // at the end of the transaction
    // dumps the status of all the table rows used
    racctperm.print_tuple();
    raddress.print_tuple();
    rcompany.print_tuple();
    rcustomer.print_tuple();
    rcustomertaxrate.print_tuple();
    rdailymarket.print_tuple();
    rexchange.print_tuple();
    rfinancial.print_tuple();
    rsecurity.print_tuple();
    rnewsitem.print_tuple();
    rnewsxref.print_tuple();
    rtaxrate.print_tuple();
    rwatchitem.print_tuple();
    rwatchlist.print_tuple();

#endif

 done:
    // return the tuples to the cache

    _paccount_permission_man->give_tuple(pracctperm);
    _paddress_man->give_tuple(praddress);
    _pcompany_man->give_tuple(prcompany);
    _pcustomer_man->give_tuple(prcustomer);
    _pcustomer_taxrate_man->give_tuple(prcustomertaxrate);
    _pdaily_market_man->give_tuple(prdailymarket);
    _pexchange_man->give_tuple(prexchange);
    _pfinancial_man->give_tuple(prfinancial);
    _psecurity_man->give_tuple(prsecurity);
    _pnews_item_man->give_tuple(prnewsitem);
    _pnews_xref_man->give_tuple(prnewsxref);
    _ptaxrate_man->give_tuple(prtaxrate);
    _pwatch_item_man->give_tuple(prwatchitem);
    _pwatch_list_man->give_tuple(prwatchlist);


    return (e);
}

EXIT_NAMESPACE(tpce);    
