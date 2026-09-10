#include "kernel_scheduler/domain/pcb.h"

#include <criterion/criterion.h>

#include "utils/collections/list.h"

Test(ks_pcb, create_pcb_sets_the_initial_state_and_priority)
{
  t_pcb* pcb = create_pcb(EST_NEW, 3);
  cr_assert_eq(get_state_pcb(pcb), EST_NEW);
  cr_assert_eq(get_priority_pcb(pcb), 3);
  cr_assert_eq(pcb->active_instances, 0);
  destroy_pcb(pcb);
}

Test(ks_pcb, pids_are_handed_out_in_order)
{
  t_pcb* first = create_pcb(EST_NEW, 0);
  t_pcb* second = create_pcb(EST_NEW, 0);
  cr_assert_eq(second->pid, first->pid + 1);
  destroy_pcb(first);
  destroy_pcb(second);
}

Test(ks_pcb, active_instance_counter_goes_up_and_down)
{
  t_pcb* pcb = create_pcb(EST_READY, 0);
  incrementar_instances_active_pcb(pcb);
  incrementar_instances_active_pcb(pcb);
  cr_assert_eq(pcb->active_instances, 2);
  disminuir_instances_active_pcb(pcb);
  cr_assert_eq(pcb->active_instances, 1);
  destroy_pcb(pcb);
}

Test(ks_pcb, blocking_mutex_is_stored_and_read_back)
{
  t_pcb* pcb = create_pcb(EST_READY, 0);
  int marker;
  cr_assert_null(get_mutex_blocking(pcb));
  set_mutex_blocking(pcb, &marker);
  cr_assert_eq(get_mutex_blocking(pcb), &marker);
  destroy_pcb(pcb);
}

Test(ks_pcb, insert_pcb_in_orden_keeps_the_list_sorted_by_priority)
{
  t_list* list = list_create();
  t_pcb* low = create_pcb(EST_READY, 5);
  t_pcb* high = create_pcb(EST_READY, 1);
  t_pcb* mid = create_pcb(EST_READY, 3);

  insert_pcb_in_orden(list, low);
  insert_pcb_in_orden(list, high);
  cr_assert_eq(insert_pcb_in_orden(list, mid),
               1); /* lands between high and low */

  cr_assert_eq(list_get(list, 0), high);
  cr_assert_eq(list_get(list, 1), mid);
  cr_assert_eq(list_get(list, 2), low);

  list_destroy(list);
  destroy_pcb(low);
  destroy_pcb(high);
  destroy_pcb(mid);
}
