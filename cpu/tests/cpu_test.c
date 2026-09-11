#include "cpu/cpu.h"

#include <criterion/criterion.h>

#include "cpu/cleanup.h"

/* ── decode_stage ──────────────────────────────────────────────────────── */

Test(cpu_decode, a_bare_instruction_has_no_parameters)
{
  t_instruction* instruction = decode_stage("NOOP");
  cr_assert_str_eq(instruction->name, "NOOP");
  cr_assert_eq(instruction->parameter_count, 0);
  destroy_instruction(instruction);
}

Test(cpu_decode, parameters_are_split_on_spaces)
{
  t_instruction* instruction = decode_stage("SET AX 5");
  cr_assert_str_eq(instruction->name, "SET");
  cr_assert_eq(instruction->parameter_count, 2);
  cr_assert_str_eq(instruction->parameters[0], "AX");
  cr_assert_str_eq(instruction->parameters[1], "5");
  destroy_instruction(instruction);
}

Test(cpu_decode, keeps_up_to_three_parameters)
{
  t_instruction* instruction = decode_stage("COPY_MEM a b c");
  cr_assert_eq(instruction->parameter_count, 3);
  cr_assert_str_eq(instruction->parameters[2], "c");
  destroy_instruction(instruction);
}
