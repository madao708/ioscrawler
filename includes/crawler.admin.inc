<?php

require_once(drupal_get_path('module','querypath').'/querypath.module');
require_once(drupal_get_path('module','querypath').'/QueryPath/QueryPath.php');

/**
 * Implementation hook_menu().
 */

function crawler_menu() {
	$items['admin/config/crawler'] = array (
		'title' => 'crawler app',
		'description' => 'crawler by store id',
		'position' => 'right',
		'page callback' => 'drupal_get_form',
		'page arguments' => array (
			'crawler_by_store_id_form'
		),
		'access callback' => TRUE,

	);
	
	return $items;
}


/**
 * 根据store id 采集
 */
function crawler_by_store_id_form() {
	$form['store_id'] = array (
		'#type' => 'textfield',
		'#title' => t('Store Id'),
		'#description' => t('The app in apple store id'),
		'#default_value' => variable_get('store_id', ''),

	);

	$form['submit'] = array (
		'#type' => 'submit',
		'#value' => t('Submit crawler'),

	);

	return $form;
}

/**
 * 验证
 */
function crawler_by_store_id_form_validate($form, $form_state) {
	if (empty ($form_state['values']['store_id'])) {
		form_set_error('store_id', 'You must enter a store id');
	}
}

/**
 * 提交
 * SELECT * FROM html WHERE url="https://itunes.apple.com/cn/app/idxxxxxxx" 
 * AND xpath="//div[@class='padder']"&format=json 
 * 
 */
function crawler_by_store_id_form_submit($form, $form_state) {
	$store_id = intval($form_state['values']['store_id']);

	$url = 'SELECT * FROM html WHERE url="https://itunes.apple.com/cn/app/id'.$store_id.'" AND xpath="//div[@class=\'padder\']"';
	$url = rawurlencode($url);
	$url = str_replace('%2A', '*', $url);
	$url = $url.'&format=xml';
	
	$result = crawler_build_query($url);
	
	if (empty ($result)) {
		watchdog('crawler', 'crawler result is null');
		drupal_set_message(t('crawler fail,result null,retry again'), 'status');
		return;
	}
	
	//根据采集到的信息保存
	var_dump($result);
	exit;

}

/**
 * 该方法移植yql模块
 * Take a query and built it into a fully qualified URL
 */

function crawler_build_query($query) {
	$yql_url = variable_get('yql_service_url', YQL_URL);
	$url = $yql_url . $query;
	$results = crawler_fetch_data($url);
	if ($results) {
		return $results;
	} else {
		return NULL;
	}
}

/**
 * Fetch data from the YQL URL
 */

function crawler_fetch_data($query) {
	if ($query) {
		$results = drupal_http_request($query);
        
		if (!isset ($results->error) && $results->data) {
			watchdog('crawler', 'Successfully parsed some text from the data using YQL.');
			$results = crawler_parse_results($results->data);
		}
		elseif (isset ($results->error)) {
			$error_message = t('YQL query fail.error code:' . $results->error);
			watchdog('crawler', $error_message);
			drupal_set_message(t('@error_message', array (
				'@error_message' => $error_message
			)), 'notice');
			return FALSE;
		}
	} else {
		watchdog('crawler', 'No query was sent to the fetch data function.');
		return;
	}
	return $results;
}

/**
 * 解析结果
 */
function crawler_parse_results($results, $error = FALSE) {
	$errors = NULL;
	$terms = array();
	$xml = $results;

	if (!$error) {

		$terms['name'] = qp($xml, 'h1')->text();
		$terms['detail'] = qp($xml, '.center-stack')->children('.product-review')->eq(0)->text();
		$terms['new'] = qp($xml, '.center-stack')->children('.product-review')->eq(1)->text();
		$terms['icon'] = qp($xml, '.product')->children('.artwork')->find('img')->attr('src');
		$terms['platform'] = qp($xml, '.app-requirements')->parent()->html();
		$terms['grade'] = qp($xml, '.app-rating')->find('a')->text();
		$terms['feetype'] = qp($xml, '.price')->find('p')->text();
		$terms['time'] = qp($xml, '.release-date')->find('p')->text();
		$terms['language'] = qp($xml, '.language')->find('p')->text();
		$terms['developer'] = qp($xml, '.intro')->find('h2')->text();
	
		foreach (qp($xml,'.iphone-screen-shots')->find('img') as $img) {
		  $terms['iphone_pic'][] = $img->attr('src');
		}
		
		foreach (qp($xml,'.ipad-screen-shots')->find('img') as $img) {
		  $terms['ipad_pic'][] = $img->attr('src');
		}
		
		return $terms;
	} else {
		if (!empty ($results)) {
			$errors .= $results;
		} else {
			drupal_set_message(t('Probably an HTTP or response error. Error data returned is @errors', array (
				'@errors' => $errors
			)), 'error');
		}
		return $errors;
	}
}
