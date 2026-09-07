#include "cpu/handlers.h"

#include <criterion/criterion.h>

#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "support.h"

/* These handlers only touch the context's registers, so the cpu and the pid are
 * placeholders. */

static t_instruction make_instruction(char* name, char* p0, char* p1)
{
  t_instruction instruction = {.name = name, .parameter_count = 0};
  if (p0 != NULL)
  {
    instruction.parameters[0] = p0;
    instruction.parameter_count++;
  }
  if (p1 != NULL)
  {
    instruction.parameters[1] = p1;
    instruction.parameter_count++;
  }
  return instruction;
}

Test(cpu_handlers, noop_does_nothing_and_succeeds)
{
  t_cpu cpu = {0};
  t_context* context = cpu_make_context();
  context->registers->EAX = 5;

  t_instruction instruction = make_instruction("NOOP", NULL, NULL);
  cr_assert_eq(handler_noop(&cpu, context, &instruction, 1), EB_TRUE);
  cr_assert_eq(context->registers->EAX, 5);

  cpu_destroy_context(context);
}

Test(cpu_handlers, set_writes_a_literal_into_a_register)
{
  t_cpu cpu = {0};
  t_context* context = cpu_make_context();

  t_instruction instruction = make_instruction("SET", "EAX", "42");
  cr_assert_eq(handler_set(&cpu, context, &instruction, 1), EB_TRUE);
  cr_assert_eq(get_register(context->registers, "EAX"), 42);

  cpu_destroy_context(context);
}

Test(cpu_handlers, sum_adds_the_second_register_into_the_first)
{
  t_cpu cpu = {0};
  t_context* context = cpu_make_context();
  set_register(context->registers, "EAX", 10);
  set_register(context->registers, "EBX", 32);

  t_instruction instruction = make_instruction("SUM", "EAX", "EBX");
  cr_assert_eq(handler_sum(&cpu, context, &instruction, 1), EB_TRUE);
  cr_assert_eq(get_register(context->registers, "EAX"), 42);
  cr_assert_eq(get_register(context->registers, "EBX"), 32);

  cpu_destroy_context(context);
}

Test(cpu_handlers, sub_subtracts_the_second_register_from_the_first)
{
  t_cpu cpu = {0};
  t_context* context = cpu_make_context();
  set_register(context->registers, "EAX", 50);
  set_register(context->registers, "EBX", 8);

  t_instruction instruction = make_instruction("SUB", "EAX", "EBX");
  cr_assert_eq(handler_sub(&cpu, context, &instruction, 1), EB_TRUE);
  cr_assert_eq(get_register(context->registers, "EAX"), 42);

  cpu_destroy_context(context);
}

Test(cpu_handlers, jnz_jumps_when_the_register_is_non_zero)
{
  t_cpu cpu = {0};
  t_context* context = cpu_make_context();
  set_register(context->registers, "EAX", 1);
  set_register(context->registers, "PC", 5);

  t_instruction instruction = make_instruction("JNZ", "EAX", "99");
  cr_assert_eq(handler_jnz(&cpu, context, &instruction, 1), EB_TRUE);
  cr_assert_eq(get_register(context->registers, "PC"), 99);

  cpu_destroy_context(context);
}

Test(cpu_handlers, jnz_does_not_jump_when_the_register_is_zero)
{
  t_cpu cpu = {0};
  t_context* context = cpu_make_context();
  set_register(context->registers, "EAX", 0);
  set_register(context->registers, "PC", 5);

  t_instruction instruction = make_instruction("JNZ", "EAX", "99");
  cr_assert_eq(handler_jnz(&cpu, context, &instruction, 1), EB_TRUE);
  cr_assert_eq(get_register(context->registers, "PC"), 5);

  cpu_destroy_context(context);
}
