<?php

/**
 * @file
 * The controller for the customer profile entity containing the CRUD operations.
 */

/**
 * The controller class for customer profiles contains methods for the profile
 * CRUD operations. The load method is inherited from the default controller.
 */
class CowProCustomerProfileEntityController extends DrupalCowProEntityController {

  /**
   * Create a default customer profile.
   *
   * @param array $values
   *   An array of values to set, keyed by property name.
   *
   * @return
   *   A customer profile object with all default fields initialized.
   */
  public function create(array $values = array()) {
    $values += array(
      'profile_id' => NULL,
      'revision_id' => NULL,
      'type' => '',
      'uid' => '',
      'status' => 1,
    	'verified' => 0,
      'created' => '',
      'changed' => '',
    );
    $hook = 'cowpro_customer_profile_' . $values['type'] . '_create';
    foreach (module_implements($hook) as $module) {
    	$values = module_invoke($module, $hook, $values);
    }

    return parent::create($values);
  }

  /**
   * Saves a customer profile.
   *
   * When saving a profile without an ID, this function will create a new
   * profile at that time. Subsequent profiles that should be saved as new
   * revisions should set $profile->revision to TRUE and include a log string in
   * $profile->log.
   *
   * @param $profile
   *   The full customer profile object to save.
   * @param $transaction
   *   An optional transaction object.
   *
   * @return
   *   SAVED_NEW or SAVED_UPDATED depending on the operation performed.
   */
  public function save($profile, DatabaseTransaction $transaction = NULL) {
    if (!isset($transaction)) {
      $transaction = db_transaction();
      $started_transaction = TRUE;
    }

    try {
      global $user;

      // Determine if we will be inserting a new profile.
      $profile->is_new = empty($profile->profile_id);

      // Set the timestamp fields.
      if ($profile->is_new && empty($profile->created)) {
        $profile->created = REQUEST_TIME;
      }
      else {
        // Otherwise if the profile is not new but comes from an entity_create()
        // or similar function call that initializes the created timestamp and
        // uid value to empty strings, unset them to prevent destroying existing
        // data in those properties on update.
        if ($profile->created === '') {
          unset($profile->created);
        }
        if ($profile->uid === '') {
          unset($profile->uid);
        }
      }

      $profile->changed = REQUEST_TIME;

      $profile->revision_uid = $user->uid;
      $profile->revision_timestamp = REQUEST_TIME;

      if ($profile->is_new || !empty($profile->revision)) {
        // When inserting either a new profile or revision, $profile->log must
        // be set because {cowpro_customer_profile_revision}.log is a text
        // column and therefore cannot have a default value. However, it might
        // not be set at this point, so we ensure that it is at least an empty
        // string in that case.
        if (!isset($profile->log)) {
          $profile->log = '';
        }
      }
      elseif (empty($profile->log)) {
        // If we are updating an existing profile without adding a new revision,
        // we need to make sure $profile->log is unset whenever it is empty. As
        // long as $profile->log is unset, drupal_write_record() will not attempt
        // to update the existing database column when re-saving the revision.
        unset($profile->log);
      }

      $hook = 'cowpro_customer_profile_' . $profile->type . '_save';
      foreach (module_implements($hook) as $module) {
      	$profile = module_invoke($module, $hook, $profile);
      }

      return parent::save($profile, $transaction);
    }
    catch (Exception $e) {
      if (!empty($started_transaction)) {
        $transaction->rollback();
        watchdog_exception($this->entityType, $e);
      }
      throw $e;
    }
  }

  /**
   * Unserializes the data property of loaded customer profiles.
   */
  public function attachLoad(&$queried_profiles, $revision_id = FALSE) {
    foreach ($queried_profiles as $profile_id => &$profile) {
    	//参考：
    	//https://www.toolmao.com/php-unserialize-error-at-offset
    	//https://stackoverflow.com/questions/10152904/unserialize-function-unserialize-error-at-offset
    	//$profile->data = preg_replace_callback('#s:(\d+):"(.*?)";#s',function($match){return 's:'.strlen($match[2]).':"'.$match[2].'";';}, $profile->data);
    	$profile->data = unserialize($profile->data);

      $hook = 'cowpro_customer_profile_' . $profile->type . '_load';
      foreach (module_implements($hook) as $module) {
      	$profile = module_invoke($module, $hook, $profile);
      }
    }

    // Call the default attachLoad() method. This will add fields and call
    // hook_cowpro_customer_profile_load().
    parent::attachLoad($queried_profiles, $revision_id);
  }

  /**
   * Deletes multiple customer profiles by ID.
   *
   * @param $profile_ids
   *   An array of customer profile IDs to delete.
   * @param $transaction
   *   An optional transaction object.
   * @param $entity_context
   *   An optional entity context array that specifies the entity throgh whose
   *   customer profile reference field the given profiles are being deleted:
   *   - entity_type: the type of entity
   *   - entity_id: the unique ID of the entity
   *
   * @return
   *   TRUE on success, FALSE otherwise.
   */
  public function delete($profile_ids, DatabaseTransaction $transaction = NULL, $entity_context = array()) {
    if (!empty($profile_ids)) {
      $profiles = $this->load($profile_ids, array());

      // Ensure the customer profiles can actually be deleted.
      foreach ((array) $profiles as $profile_id => $profile) {
        // If we received an entity context for this profile, add it now.
        if (!empty($entity_context)) {
          $profile->entity_context = $entity_context;
        }

        // If the profile cannot be deleted, remove it from the profiles array.
        if (!cowpro_customer_profile_can_delete($profile)) {
          unset($profiles[$profile_id]);
        }
      }

      // If none of the specified profiles can be deleted, return FALSE.
      if (empty($profiles)) {
        return FALSE;
      }

      parent::delete($profile_ids, $transaction);
      return TRUE;
    }
    else {
      return FALSE;
    }
  }
}
