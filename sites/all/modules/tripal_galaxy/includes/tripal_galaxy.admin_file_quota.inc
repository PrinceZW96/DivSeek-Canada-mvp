<?php

/**
 * Form for adding a file quota for a user.
 **/
function tripal_galaxy_admin_add_custom_form($form, &$form_state) {

  $username = '';
  $default_quota = variable_get('tripal_galaxy_default_file_quota', pow(20,6));
  $default_expiration = variable_get('tripal_galaxy_default_file_expiration', '60');

  if (array_key_exists('values', $form_state)) {
    $username = $form_state['values']['username'];
    $default_quota = $form_state['values']['default_quota'];
    $default_expiration = $form_state['values']['default_expiration_date'];
  }


  // Textfield (ajax call based off of existing users) for users on the site
  $form['username'] = array (
    '#type' => 'textfield',
    '#title' => 'User',
    '#autocomplete_path' => 'admin/tripal/extension/galaxy/quota/user/autocomplete',
    '#default_value' => $username,
  );

  // Custom quota textfield (prepopulated with defualt value)
  $form['quota'] = array (
    '#type' => 'textfield',
    '#title' => 'Custom User Quota',
    '#description' => 'Set the number of megabytes that a user can consume. The number must be followed by the suffix "MB" (megabytes) or "GB" (gigabytes) with no space between the number and the suffix (e.g.: 200MB).',
    '#default_value' => tripal_galaxy_format_bytes($default_quota),
  );

  // Custom exp date textfield (prepopulated with defualt value)
  $form['expiration'] = array (
    '#type' => 'textfield',
    '#title' => 'Days to Expire',
    '#description' => 'The number of days that a user uploaded file can remain on the server before it is automatically removed.',
    '#default_value' => $default_expiration,
  );

  // Submit button
  $form['button'] = array (
    '#type' => 'submit',
    '#value' => t('Submit'),
  );
  $form['cancel'] = array(
    '#type' => 'markup',
    '#markup' => l('Cancel', 'admin/tripal/extension/galaxy/quota'),
  );

  return $form;

}

/**
 * Validates the tripal_galaxy_admin_add_custom_form form.
 **/
function tripal_galaxy_admin_add_custom_form_validate($form, &$form_state) {

  $username = $form_state['values']['username'];
  $quota = $form_state['values']['quota'];
  $expiration = $form_state['values']['expiration'];

  // Make sure the username is a valid user.
  $sql = "SELECT uid FROM {users} WHERE name = :name";
  $uid = db_query($sql,array(':name' => $username))->fetchField();
  if (!$uid) {
    form_set_error('username', 'Cannot find this username');
  }

  // Does a quota already exist for this user? If so, then don't add it again
  $check = db_select('tripal_galaxy_custom_quota', 'tgcq')
    ->fields('tgcq', array('uid'))
    ->condition('uid', $uid)
    ->execute()
    ->fetchField();
  if ($check) {
    form_set_error('username', 'The user "' . $username . '" already has a custom quota set.');
  }

  // Validate the quota string.
  if (!preg_match("/^\d+(MB|GB|TB)$/", $quota)) {
    form_set_error('quota', t('Please provide a quota size in the format indicated.'));
  }

  // Validate the expiration time.
  if (!preg_match("/^\d+$/", $expiration)) {
    form_set_error('expiration', t('Please providate a positive non-decimal numeric value for the days to expire'));
  }
}


/**
 * Submiter for the tripal_galaxy_admin_add_custom_form form.
 **/
function tripal_galaxy_admin_add_custom_form_submit($form, &$form_state) {

  $username = $form_state['values']['username'];
  $quota = $form_state['values']['quota'];
  $expiration = $form_state['values']['expiration'];

  // if the 2nd element of the qutoa string occupied by a valid suffix we need to check to see
  // what we have to multiply the value by (1024 for GB 1024^2 for TB because
  // we assume that the initial number is already in MB)
  $matches = array();
  $multiplier = 'MB';
  $size = $quota;
  if (preg_match("/^(\d+)(MB|GB|TB)$/", $quota, $matches)) {
    $multiplier = $matches[2];
    $size = $matches[1];
  }

  switch ($multiplier) {
    case 'GB':
      $size = (int) $quota * pow(10,9);
      break;
    case 'TB':
      $size = (int) $quota * pow(10,12);
      break;
    default:
      $size = (int) $quota * pow(10,6);
      break;
  }

  // Get the UID of the given user.
  $sql = "SELECT uid FROM {users} WHERE name = :name";
  $uid = db_query($sql,array(':name' => $username))->fetchField();

  // Set the user quota.
  tripal_galaxy_set_user_quota($uid, $size, $expiration);

  // TODO: check to make sure that the quota was actually set, can we assume
  // it will always work?

  drupal_set_message(t('Custom quota set for the user: @username', array('@username' => $username)));
  drupal_goto('admin/tripal/extension/galaxy/quota');
}

/**
 * Edit an existing users' quota and/or expiration date
 **/
function tripal_galaxy_admin_edit_quota_form($form, &$form_state, $uid) {

  $quota = tripal_galaxy_get_user_quota($uid);
  $default_quota = $quota->custom_quota;
  $default_expiration = $quota->custom_expiration;

  if (array_key_exists('values', $form_state)) {
    $default_quota = $form_state['values']['default_quota'];
    $default_expiration = $form_state['values']['default_expiration_date'];
  }

  $user = user_load($uid);

  $form['uid'] = array(
    '#type' => 'value',
    '#value' => $uid
  );

  // Textfield (ajax call based off of existing users) for users on the site
  $form['username'] = array (
    '#type' => 'item',
    '#title' => 'User',
    '#markup' => $user->name,
  );

  // Custom quota textfield (prepopulated with defualt value)
  $form['quota'] = array (
    '#type' => 'textfield',
    '#title' => 'Custom User Quota',
    '#description' => 'Set the number of megabytes that a user can consume. The number must be followed by the suffix "MB" (megabytes) or "GB" (gigabytes) with no space between the number and the suffix (e.g.: 200MB).',
    '#default_value' => tripal_galaxy_format_bytes($default_quota),
  );

  // Custom exp date textfield (prepopulated with defualt value)
  $form['expiration'] = array (
    '#type' => 'textfield',
    '#title' => 'Days to Expire',
    '#description' => 'The number of days that a user uploaded file can remain on the server before it is automatically removed.',
    '#default_value' => $default_expiration,
  );

  // Submit button
  $form['button'] = array (
    '#type' => 'submit',
    '#value' => t('Submit'),
  );
  $form['cancel'] = array(
    '#type' => 'markup',
    '#markup' => l('Cancel', 'admin/tripal/extension/galaxy/quota'),
  );

  return $form;
}

/**
 * Same validate as the add user with the exception of no duplicate entry
 **/
function tripal_galaxy_admin_edit_quota_form_validate($form, &$form_state){

  $uid = $form_state['values']['uid'];
  $quota = $form_state['values']['quota'];
  $expiration = $form_state['values']['expiration'];

  // Validate the quota string.
  if (!preg_match("/^\d+(MB|GB|TB)$/", $quota)) {
    form_set_error('quota', t('Please provide a quota size in the format indicated.'));
  }

  // Validate the expiration time.
  if (!preg_match("/^\d+$/", $expiration)) {
    form_set_error('expiration', t('Please providate a positive non-decimal numeric value for the days to expire'));
  }
}

/**
 * Same submit as the quota overwrite function
 **/
function tripal_galaxy_admin_edit_quota_form_submit($form, &$form_state) {
  $uid = $form_state['values']['uid'];
  $quota = $form_state['values']['quota'];
  $expiration = $form_state['values']['expiration'];

  // if the 2nd element of the qutoa string occupied by a valid suffix we need to check to see
  // what we have to multiply the value by (1024 for GB 1024^2 for TB because
  // we assume that the initial number is already in MB)
  $matches = array();
  $multiplier = 'MB';
  $size = $quota;
  if (preg_match("/^(\d+)(MB|GB|TB)$/", $quota, $matches)) {
    $multiplier = $matches[2];
    $size = $matches[1];
  }

  switch ($multiplier) {
    case 'GB':
      $size = (int) $quota * pow(10,9);
      break;
    case 'TB':
      $size = (int) $quota * pow(10,12);
      break;
    default:
      $size = (int) $quota * pow(10,6);
      break;
  }

  // Set the user quota.
  tripal_galaxy_remove_user_quota($uid);
  tripal_galaxy_set_user_quota($uid, $size, $expiration);

  $user = user_load($uid);

  drupal_set_message(t('Custom quota set for the user: @username', array('@username' => $user->name)));
  drupal_goto('admin/tripal/extension/galaxy/quota');
}


/**
 * Provides the means of setting quotas and seeing server consumption.
 *
 * @return A table of the current users with custom quotas, fieldsets for
 *         adding new users to the custom quotas, and fieldsets for setting
 *         site wide quota and expiration date.
 */
function tripal_galaxy_admin_manage_quota_form($form, &$form_state) {
  // Provide overall server consumption (and space remaining)
  $total_size = 0;

  $default_quota = variable_get('tripal_galaxy_default_file_quota', pow(20,6));
  $default_expiration = variable_get('tripal_galaxy_default_file_expiration', '60');

  if (array_key_exists('values', $form_state)) {
    $default_quota = $form_state['values']['default_quota'];
    $default_expiration = $form_state['values']['default_expiration_date'];
  }

  // Query file usage table for the fids that the galaxy module uses
  // Iterate through all of the files managed by the tripal_galaxy module
  // and calculate the total.
  $sql = "SELECT DISTINCT fid FROM {file_usage} WHERE module = 'tripal_galaxy'";
  $fids = db_query($sql);
  while($fid = $fids->fetchObject()) {
    $sql = "SELECT filesize FROM {file_managed} WHERE fid = :fid";
    $total_size += db_query($sql, array (':fid' => $fid->fid))->fetchObject()->filesize;
  }
  $form['total_size'] = array (
    '#type' => 'item',
    '#title' => t('Total Current Usage'),
    '#description' => t('The total amount of space consumed by user file uploads.'),
    '#markup' => tripal_galaxy_format_bytes($total_size),
  );

  // TODO: add a D3 chart showing the amount of storage used by each user.

  $form['default_quota'] = array (
    '#type' => 'textfield',
    '#title' => 'Default System-Wide User Quota',
    '#description' => 'Set the number of megabytes that a user can consume. The number must be followed by the suffix "MB" (megabytes) or "GB" (gigabytes) with no space between the number and the suffix (e.g.: 200MB).',
    '#default_value' => tripal_galaxy_format_bytes($default_quota),
  );

  $form['default_expiration_date'] = array (
    '#type' => 'textfield',
    '#title' => 'Default System-Wide Expiration Date',
    '#description' => 'The number of days that a user uploaded file can remain on the server before it is automatically removed',
    '#default_value' => $default_expiration,
  );

  // Populate the table from the custom quota db table (users, quota, exp date).
  $header = array(
    'uid' => t('UID'),
    'user' => t('Users'),
    'custom_quota' => t('Custom Quota'),
    'exp_date' => t('Expiration Date'),
    'actions' => t('Actions'),
  );

  // API call to the gather the users that have a custom quota
  $rows = array();
  $query = "SELECT * FROM {tripal_galaxy_custom_quota}";
  $data = db_query($query);
  while($entry = $data->fetchObject()) {
    $user = user_load($entry->uid);
    $rows[] = array(
      'uid' => $user->uid,
      'user' => $user->name,
      'custom_quota' => tripal_galaxy_format_bytes($entry->custom_quota),
      'exp_date' => $entry->custom_expiration,
    );
  }

  // Add the Actions links
  foreach($rows as $key => $entry){
    $rows[$key]['actions'] = l('Edit', 'admin/tripal/extension/galaxy/quota/edit/' . $entry['uid']) . ' | ' .
      l('Remove', 'admin/tripal/extension/galaxy/quota/remove/' . $entry['uid']);
  }

  $form['custom'] = array(
    '#type' => 'fieldset',
    '#title' => 'Custom Settings',
    '#description' => t('The settings above apply to all users who use Galaxy workflows.  The following allows for custom user settings that override the defaults set above.'),
    '#collapsed' => TRUE,
    '#collapsible' => FALSE,
  );

  $form['custom']['links'] = array(
    '#type' => 'markup',
    '#markup' => '<br>' . l('Add Custom User Quota', 'admin/tripal/extension/galaxy/quota/add'),
  );

  $form['custom']['custom_quotas'] = array(
    '#type' => 'item',
    '#title' => t('Custom User Quotas'),

    '#markup' => theme_table(array(
      'header' => $header,
      'rows' => $rows,
      'attributes' => array(),
      'caption' => '',
      'sticky' => TRUE,
      'empty' => 'There are no custom user quotas.',
      'colgroups' => array(),
    )),
  );

  $form['update_defaults'] = array (
    '#type' => 'submit',
    '#value' => t('Save'),
  );

  return $form;
}


/**
 * Validate the form's values: proper numbers and/or MB, GB, TB for quota field.
 **/
function tripal_galaxy_admin_manage_quota_form_validate($form, &$form_state) {
  $quota = $form_state['values']['default_quota'];
  $expiration = $form_state['values']['default_expiration_date'];

  // Validate the quota string.
  if (!preg_match("/^\d+(MB|GB|TB)$/", $quota)) {
    form_set_error('default_quota', t('Please provide a quota size in the format indicated.'));
  }

  // Validate the expiration time.
  if (!preg_match("/^\d+$/", $expiration)) {
    form_set_error('default_expiration', t('Please providate a positive non-decimal numeric value for the days to expire'));
  }

}

/**
 * Write to the two drupal variables the site wide defualt quota and exp date.
 **/
function tripal_galaxy_admin_manage_quota_form_submit($form, &$form_state) {
  $quota = $form_state['values']['default_quota'];
  $expiration = $form_state['values']['default_expiration_date'];

  // if the 2nd element of the qutoa string occupied by a valid suffix we need to check to see
  // what we have to multiply the value by (1024 for GB 1024^2 for TB because
  // we assume that the initial number is already in MB)
  $matches = array();
  $multiplier = 'MB';
  $size = $quota;
  if (preg_match("/^(\d+)(MB|GB|TB)$/", $quota, $matches)) {
    $multiplier = $matches[2];
    $size = $matches[1];
  }

  switch ($multiplier) {
    case 'GB':
      $size = (int) $quota * pow(10,9);
      break;
    case 'TB':
      $size = (int) $quota * pow(10,12);
      break;
    default:
      $size = (int) $quota * pow(10,6);
      break;
  }

  // Grab the quota value and exp_date to write to the drupal variables
  variable_set('tripal_galaxy_default_file_quota', $size);
  variable_set('tripal_galaxy_default_file_expiration', $expiration);

  drupal_set_message('Default quota settings have been set.');
}

/**
 * API call to remove a user from the custom quota table
 *
 * @return Boolean value to confirm removal from the table.
 **/
function tripal_galaxy_admin_remove_quota_form($form, &$form_state, $uid) {
  $form = array();

  $form['uid'] = array(
    '#type' => 'value',
    '#value' => $uid,
  );

  $user = user_load($uid);

  return confirm_form($form,
    t('Confirm removal of the custom quota for the user: "' . $user->name . '"?'),
    'admin/tripal/extension/galaxy/quota',
    t('Removal of the custom quota will enforce default quotas for the user. If the user\'s current usage exceeds the defaults the user must then remove files before more may be uploaded.')
  );
}

/**
 * Implements submit hook for the tripal_galaxy_admin_remove_quota_form form.
 */
function tripal_galaxy_admin_remove_quota_form_submit($form, &$form_state) {
  $uid = $form_state['values']['uid'];
  tripal_galaxy_remove_user_quota($uid);

  $user = user_load($uid);
  drupal_set_message('The custom quota for user, "' . $user->name . '", has been removed.');

  drupal_goto('admin/tripal/extension/galaxy/quota');
}