#include "kernel_scheduler/mutex.h"

#include <criterion/criterion.h>
#include <stdlib.h>

#include "kernel_scheduler/misc.h"
#include "support.h"

Test(ks_mutex, create_and_add_mutex_reports_duplicates)
{
  t_mutex_list* list = init_list_mutex();

  cr_assert_eq(create_and_add_mutex(list, "m1", false, NULL), RM_MUTEX_CREATED);
  cr_assert_eq(create_and_add_mutex(list, "m1", false, NULL),
               RM_MUTEX_NAME_ALREADY_EXISTS);
  cr_assert_eq(create_and_add_mutex(list, "m2", false, NULL), RM_MUTEX_CREATED);

  destroy_list_mutex(list);
}

Test(ks_mutex, locking_or_unlocking_an_unknown_mutex_is_reported)
{
  ks_init_globals();
  t_mutex_list* list = init_list_mutex();
  t_pcb* pcb = create_pcb(EST_EXEC, 0);

  cr_assert_eq(list_mutex_lock(list, "ghost", pcb), RM_MUTEX_NAME_NOT_FOUND);
  cr_assert_eq(list_mutex_unlock(list, "ghost", pcb), RM_MUTEX_NAME_NOT_FOUND);

  destroy_pcb(pcb);
  destroy_list_mutex(list);
}

Test(ks_mutex, a_free_mutex_is_granted_to_the_first_caller)
{
  ks_init_globals();
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues(logger);
  t_mutex_list* list = init_list_mutex();
  t_pcb* pcb = create_pcb(EST_EXEC, 0);

  create_and_add_mutex(list, "m", false, queues);
  cr_assert_eq(list_mutex_lock(list, "m", pcb), RM_MUTEX_LOCKED);
  cr_assert_eq(list_mutex_unlock(list, "m", pcb), RM_MUTEX_UNLOCKED);

  destroy_pcb(pcb);
  destroy_list_mutex(list);
  free(queues);
  log_destroy(logger);
}

Test(ks_mutex, unlocking_a_mutex_you_do_not_hold_is_rejected)
{
  ks_init_globals();
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues(logger);
  t_mutex_list* list = init_list_mutex();
  t_pcb* owner = create_pcb(EST_EXEC, 0);
  t_pcb* other = create_pcb(EST_EXEC, 0);

  create_and_add_mutex(list, "m", false, queues);
  list_mutex_lock(list, "m", owner);

  cr_assert_eq(list_mutex_unlock(list, "m", other),
               RM_PROCESS_HAS_NO_LOCKED_MUTEX);

  destroy_pcb(owner);
  destroy_pcb(other);
  destroy_list_mutex(list);
  free(queues);
  log_destroy(logger);
}
