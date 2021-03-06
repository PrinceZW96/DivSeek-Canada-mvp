<?php

/**
 * @file
 * Implement a Drush interface for managing a DrupalDaemon.
 */

/**
 * Implements hook_drush_help().
 */
function drushd_drush_help($command) {
  switch ($command) {
    case 'drush:daemon':
      return dt('Manage a Daemon defined via the Daemon API.');
  }
}

/**
 * Implements hook_drush_command().
 *
 * Provide functions for management of Daemon API Daemons. These commands have
 * been designed such that they are available to all daemons created via
 * the Daemon API.
 */
function drushd_drush_command() {
  $items = array();
  $items['daemon'] = array(
    'description' => dt('Manage a Daemon defined via the Daemon API.'),
    'arguments' => array(
      'start'    => dt('Start the daemon.'),
      'status'   => dt('Display status information about the daemon.'),
      'stop'     => dt('Stop the daemon.'),
      'show-log' => dt('Show the log file.'),
    ),
    'options' => array(
      'num_lines' => dt('The number of lines of the log file to show.'),
      'child' => array(
        'hidden' => TRUE,
        'description' => dt('This option should only be passed via '
        . 'drush_invoke_process and essentially just allows my command '
        . 'to not fork bomb'),
      ),
      'notify-down' => 'Notify the site administrator via email if the daemon is not running when this command is executed. Useful when executed via cron'
    ),
    'examples' => array(
      'drush daemon start [daemon-machine-name]' => dt('Start the daemon.'),
      'drush daemon start [daemon-machine-name] --daemonize' => dt('Start the daemon in the background & detached.'),
      ' '   => '',
      'drush daemon status' => dt('Show the current status of the daemon.'),
      '  '  => '',
      'drush daemon stop [daemon-machine-name]' => dt('Stop the daemon.'),
      '   ' => '',
      'drush daemon restart' => dt('Restart the daemon.'),
      '    ' => '',
      'drush daemon show-log [daemon-machine-name]' => dt('Show the last 10 lines of the log file.'),
      'drush daemon show-log  [daemon-machine-name] --num_lines=50' => dt('Show the last 10 lines of the log file.'),
    ),
    'aliases' => array('daemon'),
    'allow-additional-options' => dt('This is needed to allow the various daemon '
    . 'names to be the second arguement'),
  );

  return $items;
}

/**
 * Drush Command for Daemonized management of Daemon API Daemons.
 *
 * @param string $action
 *   One of 'start','stop','restart',status','show-log'. Meant to indicate what
 *   you want the daemon to do.
 */
function drush_drushd_daemon($action, $daemon_machine_name = 'drush_daemon') {

  // Check the daemon machine name to ensure it's safe.
  if (!preg_match('/[a-zA-Z0-9_]+/', $daemon_machine_name)) {
    drush_set_error(dt('Your daemon machine name (@machine-name) must be a '
      . 'single alphanumeric word and may include uppercase and underscores.',
      array('@machine-name' => check_plain($daemon_machine_name))
    ));

    return;
  }

  // Pass control off based on the action supplied.
  if ($action == 'start') {
    drush_daemon_api_start_daemon($daemon_machine_name);
  }
  elseif ($action == 'stop') {
    drush_daemon_api_stop_daemon($daemon_machine_name);
  }
  elseif ($action == 'show-log') {
    drush_daemon_api_show_daemon_log($daemon_machine_name);
  }
  elseif ($action == 'status') {
    drush_daemon_api_show_daemon_status($daemon_machine_name);
  }
  else {
    drush_set_error(dt("Unrecognized action @action. Should be one of 'start', "
      . "'stop', 'status', or 'show-log'",
      array('@action' => check_plain($action))));
  }

}

/**
 * Start the specified Daemon.
 *
 * @param string $daemon_machine_name
 *   The machine name of the daemon (as specified in hook_daemon_api_info() )
 *   to be started.
 */
function drush_daemon_api_start_daemon($daemon_machine_name) {

  // First, instantiate the daemon.
  $daemon = drushd_instantiate_daemon($daemon_machine_name);

  // We always start our daemons in daemon-mode. Thus when the daemon is first
  // started from drush, we need to fork the process. However, we don't want
  // our children to fork continuously or we will end up with a fork_bomb.
  // Thus when we start our child process we pass in the "child" option which
  // tells our drush command not to fork again but instead to just run
  // the daemon.
  if (!drush_get_option('child')) {
    drush_invoke_process(
      '@self',
      'daemon',
      array('start', $daemon_machine_name),
      array('child' => TRUE),
      array('fork' => TRUE)
    );
    drush_print(dt("Use 'drush daemon status @machine-name' to check the "
      . "status of the daemon just started and 'drush daemon stop @machine-name' to stop it.\n",
      array('@machine-name' => $daemon_machine_name)));
  }
  else {
    $daemon->run();
  }
}

/**
 * Stop the specified Daemon.
 *
 * @param string $daemon_machine_name
 *   The machine name of the daemon (as specified in hook_daemon_api_info() )
 *   to be started.
 */
function drush_daemon_api_stop_daemon($daemon_machine_name) {

  $lock_file = drushd_get_daemon_lock_file($daemon_machine_name);
  if (file_exists($lock_file)) {
    $pid = shell_exec("cat $lock_file");

    if ($pid) {
      shell_exec("kill $pid");
      if (file_exists($lock_file)) { shell_exec("rm $lock_file  2> /dev/null"); }
      drush_log(dt('Stopped the Daemon'), 'ok');
    }
    else {
      drush_set_error(dt('Unable to retrieve PID'));
    }
  }
  else {
    drush_log(dt("\nThe Daemon is already stopped"), 'ok');
  }
}

/**
 * Show the specified Daemons Log.
 *
 * @param string $daemon_machine_name
 *   The machine name of the daemon (as specified in hook_daemon_api_info() )
 *   to be started.
 */
function drush_daemon_api_show_daemon_log($daemon_machine_name) {

  // First use the status file to get the name of the log file.
  // It needs to be done this way b/c the name of the log file is generated by
  // the user and is often based on the date. This allows custom daemons to
  // use different naming  rules without breaking drush functionality.
  $status_file = drushd_get_daemon_status_file($daemon_machine_name);
  if (!is_readable($status_file)) {
    drush_log(dt('The Daemon has never been run and therefore the log file is empty.'), 'ok');
    return;
  }

  $status = shell_exec("cat $status_file");
  if (!empty($status)) {
    $status = unserialize($status);

    // Once we have the name of the log file, use tail to print the
    // specified (default 10) number of lines to the screen.
    if (isset($status['Current Log File'])) {

      $log_filename = $status['Current Log File'];

      // Get the number of lines to display.
      $num_lines = drush_get_option('num_lines');
      $num_lines = ($num_lines) ? $num_lines : 10;
      if (!preg_match('/[0-9]+/', $num_lines)) {
        drush_log(
          dt('The number of lines of the log file to show must be indicated '
          . 'as an integer. Using the default of 10 lines instead.'),
          'warning'
        );
        $num_lines = 10;
      }

      // Use the unix tail command to get the most recent log messages.
      drush_print(shell_exec("tail -n $num_lines $log_filename"));
    }
    // Otherwise we have no way to know the name of the log file so
    // we can't show it :(.
    else {
      drush_set_error(
        dt('The name of the log file is not stored in the status file '
        . '(Current Log File). Tell your administrator that this may be a '
        . 'result of overriding the parent getStatusDetails().')
      );
      return;
    }
  }
  else {
    drush_log(
      dt('The Daemon has never been run and therefore the log file is empty.'),
      'ok'
    );
  }
}

/**
 * Show the Status of specified the Daemon.
 *
 * @param string $daemon_machine_name
 *   The machine name of the daemon (as specified in hook_daemon_api_info() )
 *   to be started.
 */
function drush_daemon_api_show_daemon_status($daemon_machine_name) {

  // If the user has provided the --notify-down open then we will send the administrator an
  // email through the drupal mail system.
  if (drush_get_option('notify-down')) {
    drushd_notify_admin_when_daemon_down();
    return;
  }

  // Determine whether the Daemon is running or not.
  if (drushd_is_daemon_running($daemon_machine_name)) {
    drush_print(dt("\nThe Daemon is currently running..."));
  }
  else {
    drush_print(dt("\nThe Daemon is currently stopped"));
  }

  // Get the Full Status Details.
  $status_file = drushd_get_daemon_status_file($daemon_machine_name);
  if (!$status_file) {
    drush_log(dt('The Daemon has never been run.'), 'ok');
    return;
  }

  $status = shell_exec("cat $status_file");
  if (!empty($status)) {

    // The status is stored in the file as a serialized array so unserialize it.
    $status = unserialize($status);

    // Iterate through the status array and print each element.
    drush_print(dt("\nThe last known status for this daemon is:"));
    foreach ($status as $name => $value) {
      // Check for boolean TRUE to print nicely.
      if ($value === TRUE) {
        drush_print("\t" . check_plain($name) . ": TRUE");
      }
      // Check for boolean FALSE to print nicely.
      elseif ($value === FALSE) {
        drush_print("\t" . check_plain($name) . ": FALSE");
      }
      elseif (is_array($value)) {
        if (empty($value)) {
          drush_print("\t" . check_plain($name) . ": None.");
        }
        else {
          drush_print("\t" . check_plain($name) . ": " . implode(',', $value));
        }
      }
      // Otherwise just assume it's a string and print as is.
      else {
        drush_print("\t" . check_plain($name) . ": " . check_plain($value));
      }
    }
  }
  // If the file doesn't exist we assume the Daemon has never been run.
  else {
    drush_print(dt('The Daemon has never been run.'));
  }

  drush_log('', 'ok');
}
