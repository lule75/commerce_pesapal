<?php
/**
 * This is the main callback that holds the payment iframe.
 */
function commerce_pesapal_pay() {
  global $base_url;
  
  $settings = commerce_payment_method_instance_load('commerce_pesapal|commerce_payment_commerce_pesapal');
  $consumer_key = $settings['settings']['consumer_key'];
  $consumer_secret = $settings['settings']['consumer_secret'];
  $transaction_mode = $settings['settings']['mode'];

  $token = $params = NULL;

  if ($transaction_mode = 'test') {

    $iframelink = 'http://demo.pesapal.com/api/PostPesapalDirectOrderV4';
  
  } 
  elseif ($transaction_mode = 'live') {

    $iframelink = 'https://www.pesapal.com/api/PostPesapalDirectOrderV4';
  
  }
    
  $callback_url = $base_url . '/?q=pesapal/complete';

  //Get data from the POST payload and pass that to the iframe.
  $amount = $_POST['amount'];
  $description = 'Product';//Yet to work how best to get this.
  $order_id = $_POST['order_id'];
  $type = 'MERCHANT';
  $email = $_POST['email'];
  $first_name = $_POST['firstname'];
  $last_name = $_POST['lastname'];
  $currency = $_POST['currency'];
  $phonenumber = '';//This parameter is optional.

  //Create XML to be passed to the iframe
  $post_xml  = '<?xml version="1.0" encoding="utf-8"?>';
  $post_xml .= '<PesapalDirectOrderInfo ';
  $post_xml .= 'xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" ';
  $post_xml .= 'xmlns:xsd="http://www.w3.org/2001/XMLSchema" ';
  $post_xml .= 'Amount="' . $amount . '" ';
  $post_xml .= 'Description="' . $description . '" ';
  $post_xml .= 'Type="' . $type . '" ';
  $post_xml .= 'Reference="' . $order_id . '" ';
  $post_xml .= 'FirstName="' . $first_name . '" ';
  $post_xml .= 'LastName="' . $last_name . '" ';
  $post_xml .= 'Email="' . $email . '" ';
  $post_xml .= 'PhoneNumber="' . $phonenumber . '" ';
  $post_xml .= 'Currency="' . $currency . '" ';
  $post_xml .= 'xmlns="http://www.pesapal.com" />';
  
  $post_xml =   htmlentities($post_xml);

  $consumer = new OAuthConsumer($consumer_key, $consumer_secret);

  // Construct the OAuth Request URL & post transaction to pesapal
  $signature_method = new OAuthSignatureMethod_HMAC_SHA1();

  $iframe_src = OAuthRequest::from_consumer_and_token($consumer, $token, 'GET', $iframelink, $params);
  $iframe_src -> set_parameter('oauth_callback', $callback_url);
  $iframe_src -> set_parameter('pesapal_request_data', $post_xml);
  $iframe_src -> sign_request($signature_method, $consumer, $token);

  $pesapal_iframe = '<iframe src="' . $iframe_src . '" width="100%" height="620px"  scrolling="no" frameBorder="0"><p>Browser unable to load Pesapal iFrame</p></iframe>';
   
  return $pesapal_iframe;
}

/**
  * Show a message to the user after redirection frompesapal
  */
function commerce_pesapal_complete() {
  $message = "Thank you.Your purchase is complete";

  return $message;
}

/**
  * React to Pesapal's IPN
  */
function commerce_pesapal_ipn() {
  //Confirm from pesapal.
  $settings = commerce_payment_method_instance_load('commerce_pesapal|commerce_payment_commerce_pesapal');
  $consumer_key = $settings['settings']['consumer_key'];
  $consumer_secret = $settings['settings']['consumer_secret'];

  $transaction_mode = $settings['settings']['mode'];

  if ($transaction_mode = 'test') {

    $statusrequest_api = 'http://demo.pesapal.com/api/QueryPaymentStatus';
  
  } 
  elseif ($transaction_mode = 'live') {

    $statusrequest_api = 'https://www.pesapal.com/api/QueryPaymentStatus';
  
  }

  // Fetch parameters sent by the PesaPal IPN call. PesaPal will call the URL
  // you entered in your IPN settings with the below query parameters;
  $pesapal_notification_type        = $_GET['pesapal_notification_type'];
  $pesapal_transaction_tracking_id  = $_GET['pesapal_transaction_tracking_id'];
  $pesapal_merchant_reference       = $_GET['pesapal_merchant_reference'];

  if ($pesapal_notification_type == 'CHANGE' && $pesapal_transaction_tracking_id != '') {
    // Pesapal parameters
    $token = $params = NULL;

    $consumer = new OAuthConsumer($consumer_key, $consumer_secret);

    // Get transaction status
    $signature_method = new OAuthSignatureMethod_HMAC_SHA1();
    
    $request_status = OAuthRequest::from_consumer_and_token($consumer, $token, 'GET', $statusrequest_api, $params);
    $request_status -> set_parameter('pesapal_merchant_reference', $pesapal_merchant_reference);
    $request_status -> set_parameter('pesapal_transaction_tracking_id', $pesapal_transaction_tracking_id);
    $request_status -> sign_request($signature_method, $consumer, $token);

    $ch = curl_init();
    
    curl_setopt($ch, CURLOPT_URL, $request_status);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
    curl_setopt($ch, CURLOPT_HEADER, 1);
    curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, 0);
    if (defined('CURL_PROXY_REQUIRED')) {
        if (CURL_PROXY_REQUIRED == 'True') {

            $proxy_tunnel_flag = (defined('CURL_PROXY_TUNNEL_FLAG') && strtoupper(CURL_PROXY_TUNNEL_FLAG) == 'FALSE') ? FALSE : TRUE;
            curl_setopt($ch, CURLOPT_HTTPPROXYTUNNEL, $proxy_tunnel_flag);
            curl_setopt($ch, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
            curl_setopt($ch, CURLOPT_PROXY, CURL_PROXY_SERVER_DETAILS);
        }
    }

   $response = curl_exec($ch);

   $header_size = curl_getinfo($ch, CURLINFO_HEADER_SIZE);
   $raw_header  = substr($response, 0, $header_size - 4);
   $header_array = explode('\r\n\r\n', $raw_header);
   $header      = $header_array[count($header_array) - 1];

   // Transaction status
   $elements = preg_split('/=/', substr($response, $header_size));
   $status = $elements[1]; // PENDING, COMPLETED or FAILED

   curl_close($ch);

    if ($status = 'COMPLETED') {
        commerce_pesapal_transaction_check($pesapal_transaction_tracking_id);

      if ($result==0) {
                watchdog('commerce_pesapal', 'New transaction created ' . $pesapal_merchant_reference);
                $order = commerce_order_load($pesapal_merchant_reference);
                $order_amount = $order->commerce_order_total['und']['0']['amount'];
                
                // Create Transaction.
                $transaction = commerce_payment_transaction_new('commerce_pesapal', $pesapal_merchant_reference);
                $transaction->instance_id = $settings['instance_id'];
                // // payment amount details
                $transaction->currency_code = $order->commerce_order_total['und']['0']['currency_code'];
                $transaction->amount = $order_amount;
                // // payment status
                $transaction->remote_id = $pesapal_transaction_tracking_id;
                $transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
                $transaction->message = 'Payment on PesaPal has been successful.';
                // $transaction->payload = $feedback;
                commerce_payment_transaction_save($transaction);
                commerce_payment_redirect_pane_next_page($order);

                //Insert tracking id to database to avoid duplicated transactions
                db_merge('pesapal_tracking_id')
                  ->key(array('tracking_id' => $pesapal_transaction_tracking_id))
                  ->fields(array(
                     'tracking_id' => $pesapal_transaction_tracking_id,
                    )
                  )
               ->execute();

              }
              else{
                watchdog('commerce_pesapal', 'A transaction with the same id has already been created');
              }     

        // Send Pesapal a response.
        $resp = 'pesapal_notification_type=' . $pesapal_notification_type;
        $resp .= '&pesapal_transaction_tracking_id=' . $pesapal_transaction_tracking_id;
        $resp .= '&pesapal_merchant_reference=' . $pesapal_merchant_reference;
        ob_start();
        echo $resp;
        ob_flush();
    }
    elseif ($status = 'FAILED') {

        watchdog('commerce_pesapal', 'A pesapal transaction failed');

        $resp = 'pesapal_notification_type=' . $pesapal_notification_type;
        $resp .= '&pesapal_transaction_tracking_id=' . $pesapal_transaction_tracking_id;
        $resp .= '&pesapal_merchant_reference=' . $pesapal_merchant_reference;
        ob_start();
        echo $resp;
        ob_flush();
    }

    // If PENDING ... we do nothing ... Pesapal will keep sending us the IPN but
    // at least we know it's pending

    exit;
  }

}

/**
 * Check if transaction has already been recorded.
 */
function commerce_pesapal_transaction_check($pesapal_transaction_tracking_id) {
  $query = db_select('pesapal_tracking_id', 'p');
  $query->fields('p', array('tracking_id'));
  $query->condition('p.tracking_id', $pesapal_transaction_tracking_id, '=');
      
  $result=$query->execute()->rowCount();

  return $result;
}