#include "kernel_scheduler/syscalls/mutex.h"

#include <criterion/criterion.h>
#include <stdlib.h>

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
  t_mutex_list* list = init_list_mutex();
  t_pcb* pcb = create_pcb(EST_EXEC, 0);

  cr_assert_eq(list_mutex_lock(list, "ghost", pcb), RM_MUTEX_NAME_NOT_FOUND);
  cr_assert_eq(list_mutex_unlock(list, "ghost", pcb), RM_MUTEX_NAME_NOT_FOUND);

  destroy_pcb(pcb);
  destroy_list_mutex(list);
}

Test(ks_mutex, a_free_mutex_is_granted_to_the_first_caller)
{
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

// Mirrors PRIORITY_INHERITANCE_V2's mutex chain: `low` holds MUTEX_1 and
// `mid` holds MUTEX_2 while also waiting on MUTEX_1 (so `low` inherits
// `mid`'s priority directly). When `high` then blocks on MUTEX_2 (held by
// `mid`), `mid` inherits `high`'s priority directly -- and that needs to
// propagate transitively to `low` too, since `low` is itself blocking `mid`.
Test(ks_mutex, priority_inheritance_propagates_transitively_through_a_chain)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_blocking(logger);
  t_mutex_list* list = init_list_mutex();

  t_pcb* low = create_pcb(EST_EXEC, 5);
  t_pcb* mid = create_pcb(EST_EXEC, 3);
  t_pcb* high = create_pcb(EST_EXEC, 1);

  create_and_add_mutex(list, "mutex_1", true, queues);
  create_and_add_mutex(list, "mutex_2", true, queues);

  cr_assert_eq(list_mutex_lock(list, "mutex_1", low), RM_MUTEX_LOCKED);
  cr_assert_eq(list_mutex_lock(list, "mutex_2", mid), RM_MUTEX_LOCKED);

  // mid blocks on mutex_1 (held by low): direct inheritance, not transitive.
  cr_assert_eq(list_mutex_lock(list, "mutex_1", mid), RM_WAITING_MUTEX);
  cr_assert_eq(get_priority_pcb(low), 3, "low should inherit mid's priority");

  // high blocks on mutex_2 (held by mid): direct inheritance for mid, and
  // that has to propagate through to low, since low is what's really in
  // high's way (low -> mid -> high).
  cr_assert_eq(list_mutex_lock(list, "mutex_2", high), RM_WAITING_MUTEX);
  cr_assert_eq(get_priority_pcb(mid), 1, "mid should inherit high's priority");
  cr_assert_eq(get_priority_pcb(low), 1,
               "low should transitively inherit high's priority through mid");

  destroy_pcb(low);
  destroy_pcb(mid);
  destroy_pcb(high);
  destroy_list_mutex(list);
  ks_destroy_stub_queues_blocking(queues);
  log_destroy(logger);
}

Test(ks_mutex, the_same_process_can_relock_a_mutex_it_released)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues(logger);
  t_mutex_list* list = init_list_mutex();
  t_pcb* pcb = create_pcb(EST_EXEC, 0);

  create_and_add_mutex(list, "m", false, queues);
  cr_assert_eq(list_mutex_lock(list, "m", pcb), RM_MUTEX_LOCKED);
  cr_assert_eq(list_mutex_unlock(list, "m", pcb), RM_MUTEX_UNLOCKED);
  cr_assert_eq(list_mutex_lock(list, "m", pcb), RM_MUTEX_LOCKED);

  destroy_pcb(pcb);
  destroy_list_mutex(list);
  free(queues);
  log_destroy(logger);
}

Test(ks_mutex, a_non_priority_mutex_hands_off_to_waiters_in_fifo_order)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  t_mutex_list* list = init_list_mutex();
  t_pcb* owner = create_pcb(EST_EXEC, 0);
  t_pcb* first = create_pcb(EST_EXEC, 0);
  t_pcb* second = create_pcb(EST_EXEC, 0);

  create_and_add_mutex(list, "m", false, queues);
  list_mutex_lock(list, "m", owner);
  cr_assert_eq(list_mutex_lock(list, "m", first), RM_WAITING_MUTEX);
  cr_assert_eq(list_mutex_lock(list, "m", second), RM_WAITING_MUTEX);

  list_mutex_unlock(list, "m", owner); /* first now holds it, back in READY */
  cr_assert_eq(first->state, EST_READY);
  cr_assert_eq(list_mutex_unlock(list, "m", first), RM_MUTEX_UNLOCKED);
  cr_assert_eq(second->state, EST_READY);

  /* the queue is done with them now */
  transition_take_ready_next(&(queues->ready));
  transition_take_ready_next(&(queues->ready));
  destroy_pcb(owner);
  destroy_pcb(first);
  destroy_pcb(second);
  destroy_list_mutex(list);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_mutex, an_inherited_priority_reverts_when_the_holder_unlocks)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  t_mutex_list* list = init_list_mutex();
  t_pcb* low = create_pcb(EST_EXEC, 5);
  t_pcb* high = create_pcb(EST_EXEC, 1);

  create_and_add_mutex(list, "m", true, queues);
  list_mutex_lock(list, "m", low);
  cr_assert_eq(list_mutex_lock(list, "m", high), RM_WAITING_MUTEX);
  cr_assert_eq(get_priority_pcb(low), 1, "low inherits high's priority");

  list_mutex_unlock(list, "m", low);
  cr_assert_eq(get_priority_pcb(low), 5, "low reverts to its own priority");
  cr_assert_eq(high->state, EST_READY);

  transition_take_ready_next(&(queues->ready));
  destroy_pcb(low);
  destroy_pcb(high);
  destroy_list_mutex(list);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}
