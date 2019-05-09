# EOS Thread Context Switch

## Project Notes

* POP instruction translates into LDMIA.W
  ```
  POP {R4, R5, R6, R7, R8, R9, R10, R11, LR}
  ```
  In Dissasembly mode was:
  ```
  ldmia.w sp!, {r4, r5, r6, r7, r8, r9, r10, r11, lr}
  ```
* "PUSH is a synonym for STMDB sp!, reglist and POP is a synonym for LDMIA sp! reglist. PUSH and POP are the preferred mnemonics in these cases."  
  That means that it will POP from the Stack Pointer, either Main or Process, depending on SPSEL in CONTROL Register.

* Change all `__asm` to `asm volatile` to make sure that the compiler won't delete any asm instruction.

* LDM - Load Multiple Registers.  
  STM - Store Multiple Registers.  
  -DB - Decrement Before.  
  -IA - Increment After.  
  "The PUSH and POP instructions can be expressed in this form. See PUSH and POP for details."
