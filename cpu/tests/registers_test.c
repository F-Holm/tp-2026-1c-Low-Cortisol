#include "cpu/registers.h"

#include <criterion/criterion.h>
#include <stdlib.h>

#include "utils/registers_cpu.h"

Test(cpu_registers, set_and_get_every_register)
{
  t_registers regs = {0};
  char* names[] = {"PC",  "AX",  "BX",  "CX", "DX", "EAX",
                   "EBX", "ECX", "EDX", "SI", "DI"};

  for (int i = 0; i < 11; i++)
  {
    set_register(&regs, names[i], 7 + i);
    cr_assert_eq(get_register(&regs, names[i]), (uint32_t)(7 + i),
                 "register %s", names[i]);
  }
}

Test(cpu_registers, the_8_bit_registers_truncate_the_value)
{
  t_registers regs = {0};
  set_register(&regs, "AX", 0x1FF); /* 511 -> keeps the low byte */
  cr_assert_eq(get_register(&regs, "AX"), 0xFF);
  set_register(&regs, "DX", 256);
  cr_assert_eq(get_register(&regs, "DX"), 0);
}

Test(cpu_registers, the_32_bit_registers_keep_the_full_value)
{
  t_registers regs = {0};
  set_register(&regs, "EAX", 0xDEADBEEF);
  cr_assert_eq(get_register(&regs, "EAX"), 0xDEADBEEF);
}

Test(cpu_registers, an_unknown_register_reads_as_zero_and_writes_nothing)
{
  t_registers regs = {0};
  regs.EAX = 42;
  set_register(&regs, "ZZ", 99); /* ignored */
  cr_assert_eq(get_register(&regs, "ZZ"), 0);
  cr_assert_eq(regs.EAX, 42);
}
