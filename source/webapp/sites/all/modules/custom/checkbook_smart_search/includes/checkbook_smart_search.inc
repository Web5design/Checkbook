<?php
/*
 * Redirects to the search results page upon submitting the search form
 *
*/

/** Submit handler for smart serch */
function _checkbook_smart_search_submit($form, &$form_state){
    $search_term =  trim($form['search_box']['#value']);
    if($search_term == 'Type ahead search'){$search_term = '';}
    $search_term = ($search_term)? $search_term : '';

    $form_state['redirect'] = array(
      'smart_search',
      array(
        'query' => array(
          'search_term' => urlencode($search_term),
        )
      )
    );
}

/** Returns results for smart search */
function _checkbook_smart_search_get_results(){

    $search_terms = explode('*|*', $_REQUEST['search_term']);
    $search_terms[0] = urldecode($search_terms[0]);
    if($search_terms[0] == ""){
        $search_text = "*:*";
    } else {
        $search_text = _checkbook_smart_search_escapeSolrValue($search_terms[0]);
        $search_text = ucwords(strtolower($search_text));
    }

    //Search results
    $page_no = ($_REQUEST['page'])? $_REQUEST['page'] : 0;
    $no_of_results = 10;
    $search_results = _checkbook_smart_search_results($search_terms, $page_no, $no_of_results);

    // Facet Results
    $domains = _checkbook_smart_search_get_facet_results($search_text, $search_terms, "domains");
    $agencies = _checkbook_smart_search_get_facet_results($search_text, $search_terms, "agency_names", NULL, -1);
    $vendors = _checkbook_smart_search_get_facet_results($search_text, $search_terms, "vendor_names", NULL, 100);
    $all_vendors = _checkbook_smart_search_get_facet_results($search_text, $search_terms, "vendor_names", NULL, -1);
    $vendor_number = count($all_vendors['facet_counts']['facet_fields']['vendor_name']);
    $vendor_pages = floor($vendor_number / 100);
    drupal_add_js(array('checkbook_smart_search' => array('vendor_pages' => $vendor_pages)), 'setting');
    $expense_categories = _checkbook_smart_search_get_facet_results($search_text, $search_terms, "expense_categories", NULL, 100);
    $all_expense_categories = _checkbook_smart_search_get_facet_results($search_text, $search_terms, "expense_categories", NULL, -1);
    $expense_categories_number = count($all_expense_categories['facet_counts']['facet_fields']['expenditure_object_name']);
    $expense_category_pages = floor($expense_categories_number / 100);
    drupal_add_js(array('checkbook_smart_search' => array('expense_category_pages' => $expense_category_pages)), 'setting');
    $fiscal_years = _checkbook_smart_search_get_facet_results($search_text, $search_terms, "fiscal_years");
    $contract_categories = _checkbook_smart_search_get_facet_results($search_text, $search_terms, "contract_categories");
    $contract_status = _checkbook_smart_search_get_facet_results($search_text, $search_terms, "contract_status");
    $spending_categories = _checkbook_smart_search_get_facet_results($search_text, $search_terms, "spending_categories");

    $facetResults = array("domain" => $domains['facet_counts']['facet_fields'],
                          "agency_name" => $agencies['facet_counts']['facet_fields'],
                          "vendor_name" => $vendors['facet_counts']['facet_fields'],
                          "expenditure_object_name" => $expense_categories['facet_counts']['facet_fields'],
                          "fiscal_year" => $fiscal_years['facet_counts']['facet_fields'],
                          "spending_category_name" => $spending_categories['facet_counts']['facet_fields'],
                          "contract_category_name" => $contract_categories['facet_counts']['facet_fields'],
                          "contract_status" => $contract_status['facet_counts']['facet_fields']);

    //Selected Facet Filters
    $selected_domains = _checkbook_smart_search_get_selected_facet_results($search_text, $search_terms, "domains");
    $selected_agencies = _checkbook_smart_search_get_selected_facet_results($search_text, $search_terms, "agency_names");
    $selected_vendors = _checkbook_smart_search_get_selected_facet_results($search_text, $search_terms, "vendor_names");
    $selected_expense_categories = _checkbook_smart_search_get_selected_facet_results($search_text, $search_terms, "expense_categories");
    $selected_fiscal_years = _checkbook_smart_search_get_selected_facet_results($search_text, $search_terms, "fiscal_years");
    $selected_spending_categories = _checkbook_smart_search_get_selected_facet_results($search_text, $search_terms, "spending_categories");
    $selected_contract_status = _checkbook_smart_search_get_selected_facet_results($search_text, $search_terms, "contract_status");
    $selected_contract_categories = _checkbook_smart_search_get_selected_facet_results($search_text, $search_terms, "contract_categories");

    $selected_facet_results = array("domain" => $selected_domains,
                          "agency_name" => $selected_agencies,
                          "vendor_name" => $selected_vendors,
                          "expenditure_object_name" => $selected_expense_categories,
                          "fiscal_year" => $selected_fiscal_years,
                          "spending_category_name" => $selected_spending_categories,
                          "contract_category_name" => $selected_contract_categories,
                          "contract_status" => $selected_contract_status);

    return theme('checkbook_smart_search_results', array('search_results'=> $search_results['search_results'],
                                                         'active_contracts' => $search_results['active_contracts'],
                                                         'facet_results' => $facetResults,
                                                         'selected_facet_results' => $selected_facet_results));
}

/**
 * Outputs search results using the template file 'checkbook_smart_search_results.tpl.php'.
 * @param
 * @return search results
*/
function _checkbook_smart_search_results($search_terms, $page_no, $no_of_results){
    
    $current_date = date("c").'Z';

    $search_string = _checkbook_smart_search_get_results_query_search_term();
    if($_REQUEST['page']){
      $start = (($_REQUEST['page'])*10);
    }else{
      $start = 0;
    }
     $query = "select/?q=". $search_string
               ."&start=". ($page_no*$no_of_results)
               ."&hl=true&hl.fl=*"
               ."&fq=-latest_flag:N"
               ."&wt=php";

     $search_results = _checkbook_smart_search_solr_results($query);
    
    // Active Contracts Results 
    $act_start_date_filter = "start_date:[*%20TO%20".$current_date."]";
    $act_end_date_filter = "end_date:[".$current_date."%20TO%20*]";
    $act_search_string = str_replace('&fq=(contract_status:"pending")','',$search_string);
    $active_contracts_query = "select/?q=" . $act_search_string
                               ."&fq=". $act_start_date_filter
                               ."&fq=". $act_end_date_filter
                               ."&fq=latest_flag:Y"
                               ."&fq=contract_status:registered"
                               ."&wt=php";

    $active_contracts = _checkbook_smart_search_solr_results($active_contracts_query);

    return array('search_results'=> $search_results,'active_contracts' => $active_contracts);
}

/** Returns solr results based on the search criteria from current URL */
function _checkbook_smart_search_get_results_query_search_term(){
  $parameterMapping = array("agency_names" => "agency_name",
      "fiscal_years" => "fiscal_year",
      "domains" => "domain",
      "vendor_names" => "vendor_name",
      "expense_categories" => "expenditure_object_name",
      "revenue_categories" => "revenue_category_name",
      "spending_categories" => "spending_category_name",
      "contract_categories" => "contract_category_name",
      "contract_status" => "contract_status");

  $string = $_REQUEST['search_term'];
  $searchTerms = explode('*|*', $string);
  $searchTerm = NULL;
  $searchTerms[0] = urldecode($searchTerms[0]);

  if($searchTerms[0] == ""){
    $searchTerm = "*:*";
  }else{
    $searchTerm = _checkbook_smart_search_escapeSolrValue($searchTerms[0]);
    $searchTerm = ucwords(strtolower($searchTerm));
  }

  for($i=1;$i < count($searchTerms);$i++){
    $searchString = NULL;
    $reqParams  = explode('=', $searchTerms[$i]);
    $filters = explode('~',urldecode($reqParams[1]));
    $subQuery = "";
    foreach($filters as $key=>$value){
      $subQuery .=  "(". $parameterMapping[$reqParams[0]] .":" . _checkbook_smart_search_escapeSolrValue('"'.$value.'"').")%20OR%20";
    }
    $subQuery = substr($subQuery, 0, -8);
    $searchTerm .= "&fq="  . $subQuery;
  }

  if($_REQUEST['page']){
    $start = (($_REQUEST['page'])*10);
  }else{
    $start = 0;
  }
  return $searchTerm;
}

/** Returns only selectes facet values from current URL */
function _checkbook_smart_search_get_selected_facet_results($search_text, $search_terms, $facet_name){
   $parameter_mapping = array(
          "agency_names" => "agency_name",
          "fiscal_years" => "fiscal_year",
          "domains" => "domain",
          "vendor_names" => "vendor_name",
          "expense_categories" => "expenditure_object_name",
          "revenue_categories" => "revenue_category_name",
          "spending_categories" => "spending_category_name",
          "contract_categories" => "contract_category_name",
          "contract_status" => "contract_status"
   );

    $search_string = $search_text;
    $request_facet_param = array();
    $result = array();

    for($i=1;$i < count($search_terms);$i++){
        $req_params  = explode('=', $search_terms[$i]);
        $filters = explode('~',urldecode($req_params[1]));
        $sub_query = "";

        foreach($filters as $key=>$value){
             if($req_params[0] == $facet_name){
                $count = 1;
                $request_facet_param[] = $value;
             }
             $sub_query .=  "(". $parameter_mapping[$req_params[0]] .":" .
            _checkbook_smart_search_escapeSolrValue('"'.$value.'"').")%20OR%20";
        }

        if(strlen($sub_query) > 0){
            $search_string .= "&fq="  . substr($sub_query, 0, -8);
        }
    }
    if($count > 0){
        $query = "select/?q=". $search_string
                      ."&facet=true"
                      ."&fq=-latest_flag:N"
                      ."&facet.sort=count"
                      ."&facet.field=".$parameter_mapping[$facet_name]
                      ."&wt=php"
                      ."&facet.mincount=1";

        $selected_facet_results = _checkbook_smart_search_solr_results($query);
        foreach($selected_facet_results['facet_counts']['facet_fields'] as $name => $value){
            if(count($value) > 0){
              foreach($request_facet_param as $a => $b){
                foreach($value as $facet=>$facet_count)
                   if($facet == $b)
                        $result[] = array('name' => $facet, 'count' => $facet_count);
              }
            }
        }
        return $result;
    }
    else
        return null;

}

/** Returns Facet results */
function _checkbook_smart_search_get_facet_results($search_text, $search_terms, $facet_name, $offset = null, $limit = null, $exclude = FALSE, $fromScroll = FALSE){

        $parameter_mapping = array(
          "agency_names" => "agency_name",
          "fiscal_years" => "fiscal_year",
          "domains" => "domain",
          "vendor_names" => "vendor_name",
          "expense_categories" => "expenditure_object_name",
          "revenue_categories" => "revenue_category_name",
          "spending_categories" => "spending_category_name",
          "contract_categories" => "contract_category_name",
          "contract_status" => "contract_status"
        );

        $search_string = $search_text;
        $termcount = 0;
        for($i=1;$i < count($search_terms);$i++){
            $req_params  = explode('=', $search_terms[$i]);
            $filters = explode('~',urldecode($req_params[1]));
            $sub_query = "";

            foreach($filters as $key=>$value){
                if($req_params[0] != $facet_name){
                    $sub_query .=  "(". $parameter_mapping[$req_params[0]] .":" .
                          _checkbook_smart_search_escapeSolrValue('"'.$value.'"').")%20OR%20";
                } elseif ($req_params[0] == $facet_name && $exclude == TRUE){
                    $sub_query .= "-".$parameter_mapping[$req_params[0]] . ":" . _checkbook_smart_search_escapeSolrValue('"'.$value.'"') . "%20OR%20";
                } elseif ($req_params[0] == $facet_name){
                  $termcount++;
                }
            }

            $sub_query = substr($sub_query, 0, -8);
            if(strlen($sub_query) > 0)
                $search_string .= "&fq="  . $sub_query;
        }
        $query = "select/?q=". $search_string
                  ."&facet=true"
                  ."&fq=-latest_flag:N"
                  ."&facet.sort=count"
                  ."&facet.field=".$parameter_mapping[$facet_name]
                  ."&wt=php"
                  ."&facet.mincount=1";

        if ($offset){
          $query .= "&facet.offset=".$offset;
        }
        if ($limit && $limit >= 0 && !$fromScroll){
          $query .= "&facet.limit=". ($limit + $termcount);
        } elseif ($limit){
          $query .= "&facet.limit=". $limit;
        }

        $facet_results = _checkbook_smart_search_solr_results($query);

        return $facet_results;
}

/**
 *  Outputs the suggestions for the given input.
 *  @param $string
 *  @return the matching words
**/
function _checkbook_smart_search_autocomplete(){

    $search_term = htmlspecialchars_decode($_REQUEST['term'], ENT_QUOTES);

    $regex1 = escapeRegex('(.* ');
    $regex2 = escapeRegex('.*)|(^');
    $regex3 = escapeRegex('.*)');
    $search_term = _checkbook_autocomplete_escapeSolrValue(strtolower($search_term));

    $search_term = $regex1.$search_term.$regex2.$search_term.$regex3;

    $query = "terms/?terms.fl=agency_name"
             ."&terms.fl=vendor_name"
             ."&terms.fl=revenue_category_name"
             ."&terms.fl=expenditure_object_name"
             ."&terms.regex=". $search_term ."&wt=php";

	$contents = _checkbook_smart_search_solr_results($query);
    $matches = array();
    $count = 0;
    foreach($contents['terms'] as $key=>$value){
        switch($key){
            case 'agency_name':
                foreach ($value as $a=>$b){
                    $count++;
                    $url = "/smart_search?search_term=*|*agency_names=";
                    array_push($matches, array("url"=>$url, "category"=>"Agencies" ,"label" => $a, "value" => urlencode($a)));
                }
                break;
            case 'vendor_name':
                foreach ($value as $a=>$b){
                    $count++;
                    $url = "/smart_search?search_term=*|*vendor_names=";
                    array_push($matches, array("url"=>$url,"category"=>"Vendors" ,"label" => $a, "value" => urlencode($a)));
                }
                break;
            /*case 'revenue_category_name':
                foreach ($value as $a=>$b){
                    $count++
                    $url = "/smart_search?search_term=*|*revenue_categories=";
                    array_push($matches, array("url"=>$url,"category"=>"Revenue Categories" ,"label" => $a, "value" => urlencode($a)));
                }
                break;*/
            case 'expenditure_object_name':
                foreach ($value as $a=>$b){
                    $count++;
                    $url = "/smart_search?search_term=*|*expense_categories=";
                    array_push($matches, array("url"=>$url,"category"=>"Expense Categories" ,"label" => $a, "value" => urlencode($a)));
                }
                break;
        }

    }
    if($count == 0){
        array_push($matches,array("url"=> "" ,"label"=>'<span>'."No matches found".'</span>', 'value' => 'No matches found'));
    }

    return drupal_json_output($matches);
}

/**
 *  Outputs query string for facet auto complete fields
 *  @param $facetName
 *  @return the Query String
**/
function _checkbook_smart_search_get_searchstring_autocomplete($facet_name,$facet_autocomplete_field){
    
    $parameter_mapping = array("agency_names" => "agency_name",
                              "fiscal_years" => "fiscal_year",
                              "domains" => "domain",
                              "vendor_names" => "vendor_name",
                              "expense_categories" => "expenditure_object_name",
                              "revenue_categories" => "revenue_category_name",
                              "spending_categories" => "spending_category_name",
                              "contract_categories" => "contract_category_name",
                              "contract_status" => "contract_status");

    $autocomplete_text = trim(htmlspecialchars_decode($_REQUEST['term'], ENT_QUOTES));
    $autocomplete_terms = explode(' ',$autocomplete_text);

    if(count($autocomplete_terms) > 0){
        $autocomplete_text = "";
        foreach($autocomplete_terms as $key => $value){
            if(strlen($value) > 0)
                $autocomplete_text .= _checkbook_autocomplete_escapeSolrValue($value)."*%20AND%20";
        }
            $autocomplete_text = "(". substr($autocomplete_text, 0, -9) . ")";
    }else{
        $autocomplete_text = _checkbook_autocomplete_escapeSolrValue($autocomplete_text)."*";
    }
    $autocomplete_text = "&fq=".$facet_autocomplete_field.":".$autocomplete_text;

    $request_search_term = $_REQUEST['search_term'];
    $search_terms = explode('*|*', $request_search_term);
    if($search_terms[0] == ""){
        $search_text = "*:*";
    }else{
        $search_text = _checkbook_smart_search_escapeSolrValue(urldecode($search_terms[0]));
        $search_text = ucwords(strtolower($search_text));
    }

    $search_string = $search_text;
    for($i=1;$i < count($search_terms);$i++){
        $req_params  = explode('=', $search_terms[$i]);
        $filters = explode('~',$value = urldecode($req_params[1]));
        $sub_query = "";
        foreach($filters as $key=>$value){
            if($req_params[0] != $facet_name){
                    $sub_query .=  "(". $parameter_mapping[$req_params[0]] .":" .
                          _checkbook_smart_search_escapeSolrValue('"'.$value.'"').")%20OR%20";
            }
        }
        $search_string .= (strlen($sub_query) > 0) ? "&fq="  . substr($sub_query, 0, -8) : "";
    }
    $query = "select/?q=". $search_string
                  .$autocomplete_text
                  ."&facet=true"
                  ."&fq=-latest_flag:N"
                  ."&facet.field=".$parameter_mapping[$facet_name]
                  ."&facet.limit=10"
                  ."&wt=php";

    $results = _checkbook_smart_search_solr_results($query);

    if ($results['response']['numFound'] != 0) {
      $facet_results = $results['facet_counts']['facet_fields'][$parameter_mapping[$facet_name]];
      $matches = array();
      foreach ($facet_results as $key => $value) {
        if ($value > 0) {
          $key = (string)$key;
          array_push($matches, array("url"=>"","category"=>$facet_name ,"label" => $key, "value" => urlencode($key)));
        }
      }
      return drupal_json_output($matches);
    }
    else {
      return drupal_json_output(array(array('label' => 'No Matches Found', 'value' => 'No Matches Found')));
    }
}
  
/**
 *  Outputs the agency suggestions for the given input.
 *  @param $string
 *  @return the matching words
**/
function _checkbook_smart_search_agency_autocomplete(){
    $agencies = _checkbook_smart_search_get_searchstring_autocomplete("agency_names","agency_name_autocomplete");
    return $agencies;
}

/**
 *  Outputs the vendor suggestions for the given input.
 *  @param $string
 *  @return the matching words
**/
function _checkbook_smart_search_vendor_autocomplete(){
    $vendors = _checkbook_smart_search_get_searchstring_autocomplete("vendor_names","vendor_name_autocomplete");
    return $vendors;
}

/**
 *  Outputs the expense category suggestions for the given input.
 *  @param $string
 *  @return the matching words
**/
function _checkbook_smart_search_expensecategory_autocomplete(){
    $expense_categories = _checkbook_smart_search_get_searchstring_autocomplete("expense_categories","expenditure_object_name_autocomplete");
    return $expense_categories;
}

/**
 *  Outputs the fiscalyear suggestions for the given input.
 *  @param $string
 *  @return the matching words
**/
function _checkbook_smart_search_fiscalyear_autocomplete(){
    $fiscal_years = _checkbook_smart_search_get_searchstring_autocomplete("fiscal_years","fiscal_year");
    return $fiscal_years;
}

/**
 *  Replaces and encodes the special characters in the goven string
 * @param $string
 * @return $string
 */
function _checkbook_smart_search_escapeSolrValue($string){

    $string = str_replace("%", "%25", $string);
    $string = str_replace("'", "%27", $string);
    $string = str_replace("\\", "%5C%5C", $string);
    $string = str_replace(" ", "%20", $string);
    $string = str_replace("#", "%23", $string);
    $string = str_replace("/", "\%2F", $string);
    $string = str_replace("&", "%26", $string);
    $string = str_replace("[", "\[", $string);
    $string = str_replace("]", "\]", $string);
    $string = str_replace(":", "\:", $string);
    $string = str_replace("+", "\%2B", $string);
    $string = str_replace("?", "\%3F", $string);
    $string = str_replace("$", "\%24", $string);
    $string = str_replace("!", "\%21", $string);
    $string = str_replace("(", "\%28", $string);
    $string = str_replace(")", "\%29", $string);
    $string = str_replace("|", "\%7C", $string);
    $string = str_replace("^", "\%5E", $string);
    $string = str_replace("*", "%2A", $string);
    $string = str_replace(".", "\%2E", $string);
    $string = str_replace("-", "\-", $string);

    return $string;
}

/** Escape special characters from regular expression string */
function escapeRegex($string){
    $string = str_replace(" ", "%20", $string);
    $string = str_replace("(", "%28", $string);
    $string = str_replace("|", "%7C", $string);
    $string = str_replace("^", "%5E", $string);
    $string = str_replace(")", "%29", $string);
    $string = str_replace("*", "%2A", $string);
    $string = str_replace(".", "%2E", $string);

    return $string;
}

/**
 *  Outputs the solr query result for the input query.
 *  @param $query
 *  @return the solr result in PHP array format.
*/
function _checkbook_smart_search_solr_results($query){
    global $conf;
    if($conf['check_book']['solr']['url']){
        $solr_server_url = $conf['check_book']['solr']['url'];
    }
    ini_set('default_socket_timeout',120);
	$url = $solr_server_url . $query;
    $contents = file_get_contents($url);
    $contents = utf8_encode($contents);
    if($contents){
        eval("\$results = " . $contents . ";");
    }
    return $results;
}

/** Returns the URL for clear functionality for each Search Term
 *  @param $string
 *  @return the substring of the input.
*/
function _checkbook_smart_search_clear_url($facetName,$facetValue=null, $count = null){

    $searchArgs = $_REQUEST['search_term'];
    $clearArgURL = "/smart_search/?search_term=";
    $clearURL = "";
    $searchTerms = explode('*|*', $searchArgs);
    if($facetName == 'search_term'){
        for($i=1;$i<count($searchTerms);$i++){
            $search_params = explode('=',$searchTerms[$i]);
            //$search_params[1] = urldecode($search_params[1]);
            $clearURL .= "*|*".$search_params[0].'='. urlencode($search_params[1]);
        }

        $clearArgURL .= $clearURL;
    }
    else{
        $clearArgURL .= $searchTerms[0];
        $contractsFlag = false;
        for($i=1;$i<count($searchTerms);$i++){
            $facets = explode('=',$searchTerms[$i]);
            if($facets[0] != $facetName){
                if($facetName == 'domains' && $facetValue == 'contracts'){
                    if($facets[0] == 'contract_categories' || $facets[0] == 'contract_status'){
                        $clearArgURL .= "";
                    }else{
                        $clearArgURL .= '*|*' . $facets[0] . '=' . urlencode($facets[1]);
                    }

                }else if($facetName == 'domains' && $facetValue == 'spending'){
                    if($facets[0] == 'spending_categories'){
                        $clearArgURL .= "";
                    }else{
                        $clearArgURL .= '*|*' . $facets[0] . '=' . urlencode($facets[1]);
                    }
                }else{
                    $clearArgURL .= '*|*' . $facets[0] . '=' . urlencode($facets[1]);
                }
            }

            if($facets[0] == 'contract_categories' || $facets[0] == 'contract_status' || $facets[0] == 'spending_categories'){
                if($facets[0] == $facetName)
                    $remove_arg = $facets[1];
            }
            if($facets[0] == 'domains' && ($facets[1] == 'contracts' || $facets[1] == 'spending')){
                $contractsFlag = true;
            }
        }

        if($contractsFlag && ($facetName != 'domains' && ($facetValue != 'contracts' || $facetName != 'spending'))){
            if($facetName == 'contract_categories' || $facetName == 'contract_status' || $facetName == 'spending_categories'){
                $contQueryString = "";

                $contArgs = explode('~', $remove_arg);
                foreach($contArgs as $key => $value){
                    if($value != urlencode($facetValue)){
                        $contQueryString .= urlencode($value)."~";
                    }
                }
                $contQueryString = substr($contQueryString, 0, -1);
                $clearArgURL .= ($count > 1) ?  ('*|*' . $facetName .'='.$contQueryString) : "";
            }
        }
    }

    return $clearArgURL;
}

/** Returns the html entities for string without replacing <em> and </em> tags.
 *  @param $string
 *  @return $string.
*/
function _checkbook_smart_search_str_html_entities($string){
    $string = str_replace("<em>", "**|**", $string);
    $string = str_replace("</em>", "**||**", $string);
    $string = htmlentities($string);
    $string = str_replace("**|**", "<em>", $string);
    $string = str_replace("**||**", "</em>", $string);

    return $string;
}

/**
 * This function returns the current NYC year  ...
 * @return year_id    $string = str_replace("^", "\%5E", $string);
 */
function getCurrentYearID(){
	STATIC $current_nyc_year;
	if(!isset($current_nyc_year)){
        $current_nyc_year=date("Y");
        $current_month=date("m");
        if($current_month <= 6 )
         $current_nyc_year -=1;
        $current_nyc_year = _getYearIDFromValue($current_nyc_year);
	 }
     return $current_nyc_year;
}

/** Generates smart search export form */
function _checkbook_smart_search_export_form(){
  echo theme('checkbook_smart_search_export_form');
}

/** Downloads the smart search export data */
function _checkbook_smart_search_export_download(){
  drupal_add_http_header("Content-Type", "text/csv");
  //drupal_add_http_header("Content-Length",9);
  drupal_add_http_header("Content-Disposition", "attachment; filename=TransactionsData.csv");
  drupal_add_http_header("Pragma", "cache");
  drupal_add_http_header("Expires", "-1");
  _checkbook_smart_search_export_data();
}

/** Exports the smart search export data */
function _checkbook_smart_search_export_data(){
  $search_terms = explode('*|*', $_REQUEST['searchTerm']);
  $index = 0;
  foreach($search_terms as $search_term){
    $terms = explode("=", $search_term);
    if($terms[0] == "domains"){
      $domains = explode("~", $terms[1] );
      $search_terms[$index] = "domains=" . $_REQUEST['domain'];
    }
    $index += 1;
  }
  global $conf;
  $remaining = $conf['check_book']['datatables']['iTotalDisplayRecords'] ;
  $page_size = 10000;
  $page_num = 0;
  $fields = _checkbook_smart_search_domain_fields($_REQUEST['domain']);
  $fl = implode(",",array_keys($fields));
  echo implode(",",array_values($fields)) . "\n";
  
  while($remaining > 0 ){
    $results = _checkbook_smart_search_get_results_csv($search_terms, $page_num , $page_size,false,$fl, true);
    $page_num += 1 ;
    $remaining = $remaining - $page_size;
    echo $results;    
    ob_flush();
    flush();
  }

}

/** Smart search results labels array */
function _checkbook_smart_search_domain_fields($domain){
  switch ($domain){
    case "spending":
      $parameter_mapping = array(       
      "spending_category_name" => "Spending Category",
      "agency_name" => "Agency",
      "department_name" => "Department",
      "vendor_name" => "Payee Name",
      "expenditure_object_name" => "Expense Category",
      "contract_number" => "Contract ID",
      "check_amount" => "Check Amount",
      "check_eft_issued_date" => "Issue Date"
      );
      break;
    case "contracts":
      $parameter_mapping = array(
      "contract_status"=>"Status",
      "contract_category_name"=>"Category",
      "vendor_name"=>"Vendor",
      "agency_name"=>"Agency",
      "contract_number"=>"Contract ID",
      "version_number"=>"Version Number",
      "contract_purpose"=>"Purpose",
      "parent_contract_number"=>"Parent Contract ID",
      "award_method_name"=>"Award Method",
      "contract_type_code"=>"Contract type",
      "original_amount"=>"Original Amount",
      "current_amount"=>"Current Amount",
      "start_date"=>"Start date",
      "end_date"=>"End Date",
      "pin"=>"PIN",
      "apt_pin"=>"APT PIN",      
      "registration_date"=>"Registration date",
      "expense_category_name"=>"Expense Category",
      "received_date"=>"Received date",
      "" => "");

      break;
    case "payroll":      
      $parameter_mapping = array(
      "civil_service_title" => "Title",
      "agency_name" => "Agency",
      "pay_frequency" => "Pay Frequency",
      "pay_date" => "Pay Date",
      "gross_pay" => "Gross Pay",
      "annual_salary" => "Annual Salary",
      "base_pay" => "Base Pay",
      "overtime_pay" => "Overtime Payments",
      "other_payments" => "Other Payments",
      "" => ""
      );
      break;
    case "budget":
    $parameter_mapping = array("agency_name" => "Agency",
        "department_name" => "Department Name",
        "expenditure_object_name" => "Expense Category",
        "budget_code_name" => "Budget Category",
        "adopted_amount" => "Adopted",
        "current_budget_amount" => "Modified",
        "pre_encumbered_amount" => "Pre Encumbered",
        "encumbered_amount" => "Encumbered",
        "accrued_expense_amount" => "Accrued Expense",
        "cash_expense_amount" => "Cash Payments",
        "post_closing_adjustment_amount" => "Post Adjustments",
        "fiscal_year" => "Year",
       );
      break;
    case "revenue":
      $parameter_mapping = array( "agency_name" => "Agency Name",
        "fiscal_year" => "Budget Fiscal Year",
        "display_fiscal_year" => "Fiscal Year",
        "revenue_category_name" => "Revenue Category",
        "revenue_source_name" => "Revenue Source",
        "funding_class_name" => "Funding Class",
        "adopted_amount" => "Adopted",
        "current_budget_amount" => "Modified",
        "posting_amount" => "Recognized",
        "" => "",
       );
      break;
  }
  return $parameter_mapping;
}

/** Exports the smart search export data */
function _checkbook_smart_search_solr_results_csv($query){
  global $conf;
  if($conf['check_book']['solr']['url']){
    $solr_server_url = $conf['check_book']['solr']['url'];
  }
  ini_set('default_socket_timeout',120);
  $url = $solr_server_url . $query;
  $contents = file_get_contents($url);
  $contents = utf8_encode($contents);
  
  return $contents;
}

/** Exports the smart search export data */
function _checkbook_smart_search_get_results_csv($search_terms,$page_no, $no_of_results,$highlighting = true , $fl = "*", $skip_header = false){
  $current_date = date("c").'Z';
  $search_string = _checkbook_smart_search_get_results_query_search_term();
  $domain = $_REQUEST['domain'];
  $query = "select/?q=". $search_string
               ."&fq=-latest_flag:N"
               ."&start=". ($page_no*$no_of_results)               
               ."&wt=csv"
               ."&rows=" . $no_of_results ;
  
  $query = $query . "&fq=domain:" . $domain;
   if($highlighting){
     $query = $query . "&hl=true&hl.fl=*&fl=" . $fl;
   }else{
     $query = $query . "&fl=" . $fl;
   }
   if($skip_header){
     $query = $query . "&csv.header=false";
   }
  return _checkbook_smart_search_solr_results_csv($query);
}

/** Returns theme for Vendor facet */
function _checkbook_smart_search_vendor_facet($page){
    $search_terms = explode('*|*', $_REQUEST['search_term']);
    $search_terms[0] = urldecode($search_terms[0]);
    if($search_terms[0] == ""){
        $search_text = "*:*";
    } else {
        $search_text = _checkbook_smart_search_escapeSolrValue($search_terms[0]);
        $search_text = ucwords(strtolower($search_text));
    }
    $limit = 100;
    $double_limit = (2 * $limit);
    if ($page == 0){
      $double_limit = 100;
    }
    $offset = 0;
    if ($page > 0){
      $offset = ( $page - 1 ) * $limit;
    }
    $results = _checkbook_smart_search_get_facet_results($search_text, $search_terms, "vendor_names", $offset, $double_limit, TRUE, TRUE);
    print theme('facet_results', array('results'=> $results['facet_counts']['facet_fields']['vendor_name'], 'name'=> 'fvendorName'));
}

/** Returns theme for Expense Category facet */
function _checkbook_smart_search_expensecategory_facet($page){
    $search_terms = explode('*|*', $_REQUEST['search_term']);
    $search_terms[0] = urldecode($search_terms[0]);
    if($search_terms[0] == ""){
        $search_text = "*:*";
    } else {
        $search_text = _checkbook_smart_search_escapeSolrValue($search_terms[0]);
        $search_text = ucwords(strtolower($search_text));
    }
    $limit = 100;
    $double_limit = (2 * $limit);
    if ($page == 0){
      $double_limit = 100;
    }
    $offset = 0;
    if ($page > 0){
      $offset = ( $page - 1 ) * $limit;
    }
    $results = _checkbook_smart_search_get_facet_results($search_text, $search_terms, "expense_categories", $offset, $double_limit, TRUE, TRUE);
    print theme('facet_results', array('results'=>$results['facet_counts']['facet_fields']['expenditure_object_name'], 'name'=> 'fexpenseCategoryName'));
}
