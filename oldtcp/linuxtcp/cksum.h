#ifndef _CKSUM_H_

#define _CKSUM_H_


#define fold_32bit_sum(sum32)                                           \
({                                                                      \
  register uint __m_sum16;                                              \
  asm ("movl %1,%0           # Fold 32-bit sum\n"                       \
       "\tshr $16,%0\n"                                                 \
       "\taddw %w1,%w0\n"                                               \
       "\tadcw $0,%w0\n"                                                \
       : "=r" (__m_sum16) : "r" (sum32), "0" (__m_sum16) : "cc");       \
  __m_sum16 == 0xffff ? 0 : __m_sum16;                                  \
})

#define ip_sum(sum32, _src, _len)                               \
do {                                                            \
  uint __m_src = (uint) (_src);                                 \
  uint __m_len = (_len);                                        \
                                                                \
  assert (sizeof (sum32) >= 4);                                 \
                                                                \
  asm ("testl %%eax,%%eax        # Clear CF\n"                  \
       "1:\n"                                                   \
       "\tmovl (%1),%%eax\n"                                    \
       "\tleal 4(%1),%1\n"                                      \
       "\tadcl %%eax,%0\n"                                      \
       "\tdecl %4\n"                                            \
       "\tjnz 1b\n"                                             \
       "\tadcl $0,%0             # Add the carry bit back in\n" \
       : "=d" (sum32), "=r" (__m_src)                           \
       : "0" (sum32), "1" (__m_src), "c" (__m_len>>2)           \
       : "memory", "cc", "eax", "ecx");                         \
                                                                \
  if (__m_len & 2) {                                            \
    asm ("addw (%1),%w0\n"                                      \
         "\tadcl $0,%0\n"                                       \
         : "=r" (sum32) : "r" (__m_src), "0" (sum32)            \
         : "cc", "memory");                                     \
    __m_src += 2;                                               \
  }                                                             \
  if (__m_len & 1)                                              \
    asm ("addb (%1),%b0\n"                                      \
         "\tadcl $0,%0\n"                                       \
         : "=r" (sum32) : "r" (__m_src), "0" (sum32)            \
         : "cc", "memory");                                     \
} while (0)

#endif
