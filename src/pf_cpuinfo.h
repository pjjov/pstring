/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides detection of x86 CPU features and other information.

    Function reference:
    - PF_HAS_<feature>
    - void pf_cpuid(uint32_t leaf, uint32_t regs[4]);
    - void pf_cpuidex(uint32_t leaf, uint32_t sub, uint32_t regs[4]);
    - void pf_cpu_vendor(char vendor[12]);
    - void pf_cpu_brand(char brand[48]);
    - int pf_cpu_logical_cores();

    SPDX-FileCopyrightText: 2025 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025 Predrag Jovanović

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
**/

#ifndef POLYFILL_CPUINFO
#define POLYFILL_CPUINFO

#include <stdint.h>

#if defined(__i686__) || defined(__i586__)   \
    || defined(__i486__) || defined(__i386__)  \
    || defined(__i386) || defined(_M_IX86)     \
    || defined(_X86_) || defined(__THW_INTEL__) \
    || defined(__x86_64__) || defined(_M_AMD64) \
    || defined(_M_X64)
    #define PF_CPU_X86

    #ifdef _MSC_VER
        #include <intrin.h>
    #elif defined(__has_include)
        #if __has_include(<cpuid.h>)
            #include <cpuid.h>
        #else
            #define PF_NO_CPUID
        #endif
    #else
        #define PF_NO_CPUID
    #endif
#endif

static void pf_cpuid(uint32_t leaf, uint32_t regs[4]) {
#ifdef PF_CPU_X86
#ifdef _MSC_VER
    __cpuid(regs, leaf);
#else
    uint32_t eax, ebx, ecx, edx;

    #ifdef PF_HAS_CPUID
    __cpuid(leaf, eax, ebx, ecx, edx);
    #else
    __asm__ __volatile__ ("cpuid\n\t"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "0" (leaf), "1" (0), "2" (0)
    );
    #endif

    regs[0] = eax;
    regs[1] = ebx;
    regs[2] = ecx;
    regs[3] = edx;
#endif
#endif
}

static void pf_cpuidex(uint32_t leaf, uint32_t sub, uint32_t regs[4]) {
#ifdef PF_CPU_X86
#if defined(_MSC_VER) || defined(__cpuidex)
    __cpuidex(regs, leaf, sub);
#else
    uint32_t eax, ebx, ecx, edx;
    __asm__ __volatile__ ("cpuid\n\t"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "0" (leaf), "1" (0), "2" (sub)
    );

    regs[0] = eax;
    regs[1] = ebx;
    regs[2] = ecx;
    regs[3] = edx;
#endif
#endif
}

static inline void pf_cpu_vendor(char vendor[12]) {
#ifdef PF_CPU_X86
    if (vendor) {
        uint32_t regs[4];
        pf_cpuid(0, regs);
        ((uint32_t *)vendor)[0] = regs[1];
        ((uint32_t *)vendor)[0] = regs[3];
        ((uint32_t *)vendor)[0] = regs[2];
    }
#endif
}

static inline void pf_cpu_brand(char brand[48]) {
#ifdef PF_CPU_X86
    uint32_t regs[4];
    pf_cpuid(0x80000000, regs);
    if (regs[0] >= 0x80000004) {
        pf_cpuid(0x80000002, (uint32_t *)&brand[0]);
        pf_cpuid(0x80000003, (uint32_t *)&brand[16]);
        pf_cpuid(0x80000004, (uint32_t *)&brand[32]);
    }
#endif
}

static inline int pf_cpu_logical_cores() {
#ifdef PF_CPU_X86
    uint32_t regs[4];
    pf_cpuid(0, regs);
    if (regs[0] >= 1) {
        pf_cpuid(1, regs);
        return (regs[1] >> 16) & 0xff;
    }
#endif
    return 0;
}

static inline int pf_cpuid_reg(int leaf, int sub, int reg) {
    uint32_t regs[4];
    pf_cpuidex(leaf, sub, regs);
    return regs[reg];
}

#define PF_HAS_SSE3         (pf_cpuid_reg(1, 0, 2) >> 0  & 1)
#define PF_HAS_PCLMULQDQ    (pf_cpuid_reg(1, 0, 2) >> 1  & 1)
#define PF_HAS_DTES64       (pf_cpuid_reg(1, 0, 2) >> 2  & 1)
#define PF_HAS_MONITOR      (pf_cpuid_reg(1, 0, 2) >> 3  & 1)
#define PF_HAS_DS_CPL       (pf_cpuid_reg(1, 0, 2) >> 4  & 1)
#define PF_HAS_VMX          (pf_cpuid_reg(1, 0, 2) >> 5  & 1)
#define PF_HAS_SMX          (pf_cpuid_reg(1, 0, 2) >> 6  & 1)
#define PF_HAS_EST          (pf_cpuid_reg(1, 0, 2) >> 7  & 1)
#define PF_HAS_TM2          (pf_cpuid_reg(1, 0, 2) >> 8  & 1)
#define PF_HAS_SSSE3        (pf_cpuid_reg(1, 0, 2) >> 9  & 1)
#define PF_HAS_CNXT_ID      (pf_cpuid_reg(1, 0, 2) >> 10 & 1)
#define PF_HAS_SDBG         (pf_cpuid_reg(1, 0, 2) >> 11 & 1)
#define PF_HAS_FMA          (pf_cpuid_reg(1, 0, 2) >> 12 & 1)
#define PF_HAS_CMPXCHG16B   (pf_cpuid_reg(1, 0, 2) >> 13 & 1)
#define PF_HAS_XTPR         (pf_cpuid_reg(1, 0, 2) >> 14 & 1)
#define PF_HAS_PDCM         (pf_cpuid_reg(1, 0, 2) >> 15 & 1)
#define PF_HAS_PCID         (pf_cpuid_reg(1, 0, 2) >> 17 & 1)
#define PF_HAS_DCA          (pf_cpuid_reg(1, 0, 2) >> 18 & 1)
#define PF_HAS_SSE4_1       (pf_cpuid_reg(1, 0, 2) >> 19 & 1)
#define PF_HAS_SSE4_2       (pf_cpuid_reg(1, 0, 2) >> 20 & 1)
#define PF_HAS_X2APIC       (pf_cpuid_reg(1, 0, 2) >> 21 & 1)
#define PF_HAS_MOVBE        (pf_cpuid_reg(1, 0, 2) >> 22 & 1)
#define PF_HAS_POPCNT       (pf_cpuid_reg(1, 0, 2) >> 23 & 1)
#define PF_HAS_TSC_DEADLINE (pf_cpuid_reg(1, 0, 2) >> 24 & 1)
#define PF_HAS_AES          (pf_cpuid_reg(1, 0, 2) >> 25 & 1)
#define PF_HAS_XSAVE        (pf_cpuid_reg(1, 0, 2) >> 26 & 1)
#define PF_HAS_OSXSAVE      (pf_cpuid_reg(1, 0, 2) >> 27 & 1)
#define PF_HAS_AVX          (pf_cpuid_reg(1, 0, 2) >> 28 & 1)
#define PF_HAS_F16C         (pf_cpuid_reg(1, 0, 2) >> 29 & 1)
#define PF_HAS_RDRND        (pf_cpuid_reg(1, 0, 2) >> 30 & 1)

#define PF_HAS_FPU    (pf_cpuid_reg(1, 0, 3) >> 0  & 1)
#define PF_HAS_VME    (pf_cpuid_reg(1, 0, 3) >> 1  & 1)
#define PF_HAS_DE     (pf_cpuid_reg(1, 0, 3) >> 2  & 1)
#define PF_HAS_PSE    (pf_cpuid_reg(1, 0, 3) >> 3  & 1)
#define PF_HAS_TSC    (pf_cpuid_reg(1, 0, 3) >> 4  & 1)
#define PF_HAS_MSR    (pf_cpuid_reg(1, 0, 3) >> 5  & 1)
#define PF_HAS_PAE    (pf_cpuid_reg(1, 0, 3) >> 6  & 1)
#define PF_HAS_MCE    (pf_cpuid_reg(1, 0, 3) >> 7  & 1)
#define PF_HAS_CX8    (pf_cpuid_reg(1, 0, 3) >> 8  & 1)
#define PF_HAS_APIC   (pf_cpuid_reg(1, 0, 3) >> 9  & 1)
#define PF_HAS_SEP    (pf_cpuid_reg(1, 0, 3) >> 11 & 1)
#define PF_HAS_MTRR   (pf_cpuid_reg(1, 0, 3) >> 12 & 1)
#define PF_HAS_PGE    (pf_cpuid_reg(1, 0, 3) >> 13 & 1)
#define PF_HAS_MCA    (pf_cpuid_reg(1, 0, 3) >> 14 & 1)
#define PF_HAS_CMOV   (pf_cpuid_reg(1, 0, 3) >> 15 & 1)
#define PF_HAS_PAT    (pf_cpuid_reg(1, 0, 3) >> 16 & 1)
#define PF_HAS_PSE_36 (pf_cpuid_reg(1, 0, 3) >> 17 & 1)
#define PF_HAS_PSN    (pf_cpuid_reg(1, 0, 3) >> 18 & 1)
#define PF_HAS_CLFSH  (pf_cpuid_reg(1, 0, 3) >> 19 & 1)
#define PF_HAS_DS     (pf_cpuid_reg(1, 0, 3) >> 21 & 1)
#define PF_HAS_ACPI   (pf_cpuid_reg(1, 0, 3) >> 22 & 1)
#define PF_HAS_MMX    (pf_cpuid_reg(1, 0, 3) >> 23 & 1)
#define PF_HAS_FXSR   (pf_cpuid_reg(1, 0, 3) >> 24 & 1)
#define PF_HAS_SSE    (pf_cpuid_reg(1, 0, 3) >> 25 & 1)
#define PF_HAS_SSE2   (pf_cpuid_reg(1, 0, 3) >> 26 & 1)
#define PF_HAS_SS     (pf_cpuid_reg(1, 0, 3) >> 27 & 1)
#define PF_HAS_HTT    (pf_cpuid_reg(1, 0, 3) >> 28 & 1)
#define PF_HAS_TM     (pf_cpuid_reg(1, 0, 3) >> 29 & 1)
#define PF_HAS_IA64   (pf_cpuid_reg(1, 0, 3) >> 30 & 1)
#define PF_HAS_PBE    (pf_cpuid_reg(1, 0, 3) >> 31 & 1)

#define PF_HAS_FSGSBASE        (pf_cpuid_reg(7, 0, 1) >> 0  & 1)
#define PF_HAS_TSC_ADJ         (pf_cpuid_reg(7, 0, 1) >> 1  & 1)
#define PF_HAS_SGX             (pf_cpuid_reg(7, 0, 1) >> 2  & 1)
#define PF_HAS_BMI1            (pf_cpuid_reg(7, 0, 1) >> 3  & 1)
#define PF_HAS_HLE             (pf_cpuid_reg(7, 0, 1) >> 4  & 1)
#define PF_HAS_AVX2            (pf_cpuid_reg(7, 0, 1) >> 5  & 1)
#define PF_HAS_FDP_EXCPTN_ONLY (pf_cpuid_reg(7, 0, 1) >> 6  & 1)
#define PF_HAS_SMEP            (pf_cpuid_reg(7, 0, 1) >> 7  & 1)
#define PF_HAS_BMI2            (pf_cpuid_reg(7, 0, 1) >> 8  & 1)
#define PF_HAS_ERMS            (pf_cpuid_reg(7, 0, 1) >> 9  & 1)
#define PF_HAS_INVPCID         (pf_cpuid_reg(7, 0, 1) >> 10 & 1)
#define PF_HAS_RTM             (pf_cpuid_reg(7, 0, 1) >> 11 & 1)
#define PF_HAS_RTD_M           (pf_cpuid_reg(7, 0, 1) >> 12 & 1)
#define PF_HAS_DEPRECATE_FPU   (pf_cpuid_reg(7, 0, 1) >> 13 & 1)
#define PF_HAS_MPX             (pf_cpuid_reg(7, 0, 1) >> 14 & 1)
#define PF_HAS_RTD_A           (pf_cpuid_reg(7, 0, 1) >> 15 & 1)
#define PF_HAS_AVX512F         (pf_cpuid_reg(7, 0, 1) >> 16 & 1)
#define PF_HAS_AVX512DQ        (pf_cpuid_reg(7, 0, 1) >> 17 & 1)
#define PF_HAS_RDSEED          (pf_cpuid_reg(7, 0, 1) >> 18 & 1)
#define PF_HAS_ADX             (pf_cpuid_reg(7, 0, 1) >> 19 & 1)
#define PF_HAS_SDICT            (pf_cpuid_reg(7, 0, 1) >> 20 & 1)
#define PF_HAS_AVX512IFMA      (pf_cpuid_reg(7, 0, 1) >> 21 & 1)
#define PF_HAS_PCOMMIT         (pf_cpuid_reg(7, 0, 1) >> 22 & 1)
#define PF_HAS_CLFLUSHOPT      (pf_cpuid_reg(7, 0, 1) >> 23 & 1)
#define PF_HAS_CLWB            (pf_cpuid_reg(7, 0, 1) >> 24 & 1)
#define PF_HAS_INTEL_PT        (pf_cpuid_reg(7, 0, 1) >> 25 & 1)
#define PF_HAS_AVX512PF        (pf_cpuid_reg(7, 0, 1) >> 26 & 1)
#define PF_HAS_AVX512ER        (pf_cpuid_reg(7, 0, 1) >> 27 & 1)
#define PF_HAS_AVX512CD        (pf_cpuid_reg(7, 0, 1) >> 28 & 1)
#define PF_HAS_SHA             (pf_cpuid_reg(7, 0, 1) >> 29 & 1)
#define PF_HAS_AVX512BW        (pf_cpuid_reg(7, 0, 1) >> 30 & 1)
#define PF_HAS_AVX512VL        (pf_cpuid_reg(7, 0, 1) >> 31 & 1)

#define PF_HAS_PREFETCHWT1     (pf_cpuid_reg(7, 0, 2) >> 0  & 1)
#define PF_HAS_AVX512VBMI      (pf_cpuid_reg(7, 0, 2) >> 1  & 1)
#define PF_HAS_UMIP            (pf_cpuid_reg(7, 0, 2) >> 2  & 1)
#define PF_HAS_PKU             (pf_cpuid_reg(7, 0, 2) >> 3  & 1)
#define PF_HAS_OSPKE           (pf_cpuid_reg(7, 0, 2) >> 4  & 1)
#define PF_HAS_WAITPKG         (pf_cpuid_reg(7, 0, 2) >> 5  & 1)
#define PF_HAS_AVX512_VBMI2    (pf_cpuid_reg(7, 0, 2) >> 6  & 1)
#define PF_HAS_CET_SS          (pf_cpuid_reg(7, 0, 2) >> 7  & 1)
#define PF_HAS_GFNI            (pf_cpuid_reg(7, 0, 2) >> 8  & 1)
#define PF_HAS_VAES            (pf_cpuid_reg(7, 0, 2) >> 9  & 1)
#define PF_HAS_VPCLMULQDQ      (pf_cpuid_reg(7, 0, 2) >> 10 & 1)
#define PF_HAS_AVX512_VNNI     (pf_cpuid_reg(7, 0, 2) >> 11 & 1)
#define PF_HAS_AVX512_BITALG   (pf_cpuid_reg(7, 0, 2) >> 12 & 1)
#define PF_HAS_TME_EN          (pf_cpuid_reg(7, 0, 2) >> 13 & 1)
#define PF_HAS_AVX512VPOPCNTDQ (pf_cpuid_reg(7, 0, 2) >> 14 & 1)
#define PF_HAS_LA57            (pf_cpuid_reg(7, 0, 2) >> 16 & 1)
#define PF_HAS_RDPID           (pf_cpuid_reg(7, 0, 2) >> 22 & 1)
#define PF_HAS_KL              (pf_cpuid_reg(7, 0, 2) >> 23 & 1)
#define PF_HAS_BUS_LOCK_DETECT (pf_cpuid_reg(7, 0, 2) >> 24 & 1)
#define PF_HAS_CLDEMOTE        (pf_cpuid_reg(7, 0, 2) >> 25 & 1)
#define PF_HAS_MOVDIRI         (pf_cpuid_reg(7, 0, 2) >> 27 & 1)
#define PF_HAS_MOVDIR64B       (pf_cpuid_reg(7, 0, 2) >> 28 & 1)
#define PF_HAS_ENQCMD          (pf_cpuid_reg(7, 0, 2) >> 29 & 1)
#define PF_HAS_SGX_LC          (pf_cpuid_reg(7, 0, 2) >> 30 & 1)
#define PF_HAS_PKS             (pf_cpuid_reg(7, 0, 2) >> 31 & 1)

#define PF_HAS_SGX_KEYS               (pf_cpuid_reg(7, 0, 3) >> 1  & 1)
#define PF_HAS_AVX512_4VNNIW          (pf_cpuid_reg(7, 0, 3) >> 2  & 1)
#define PF_HAS_AVX512_4FDICTS          (pf_cpuid_reg(7, 0, 3) >> 3  & 1)
#define PF_HAS_FS_REP_MOV             (pf_cpuid_reg(7, 0, 3) >> 4  & 1)
#define PF_HAS_UINTR                  (pf_cpuid_reg(7, 0, 3) >> 5  & 1)
#define PF_HAS_AVX512_VP2INTERSECT    (pf_cpuid_reg(7, 0, 3) >> 8  & 1)
#define PF_HAS_SRBDS_CTRL             (pf_cpuid_reg(7, 0, 3) >> 9  & 1)
#define PF_HAS_MD_CLEAR               (pf_cpuid_reg(7, 0, 3) >> 10 & 1)
#define PF_HAS_RTM_ALWAYS_ABORT       (pf_cpuid_reg(7, 0, 3) >> 11 & 1)
#define PF_HAS_TSX_FORCE_ABORT        (pf_cpuid_reg(7, 0, 3) >> 13 & 1)
#define PF_HAS_SERIALIZE              (pf_cpuid_reg(7, 0, 3) >> 14 & 1)
#define PF_HAS_HYBRID                 (pf_cpuid_reg(7, 0, 3) >> 15 & 1)
#define PF_HAS_TSXLDTRK               (pf_cpuid_reg(7, 0, 3) >> 16 & 1)
#define PF_HAS_PCONFIG                (pf_cpuid_reg(7, 0, 3) >> 18 & 1)
#define PF_HAS_ARCH_LBR               (pf_cpuid_reg(7, 0, 3) >> 19 & 1)
#define PF_HAS_CET_IBT                (pf_cpuid_reg(7, 0, 3) >> 20 & 1)
#define PF_HAS_AMX_BF16               (pf_cpuid_reg(7, 0, 3) >> 22 & 1)
#define PF_HAS_AVX512_FP16            (pf_cpuid_reg(7, 0, 3) >> 23 & 1)
#define PF_HAS_AMX_TILE               (pf_cpuid_reg(7, 0, 3) >> 24 & 1)
#define PF_HAS_AMX_INT8               (pf_cpuid_reg(7, 0, 3) >> 25 & 1)
#define PF_HAS_IBRS_IBPB              (pf_cpuid_reg(7, 0, 3) >> 26 & 1)
#define PF_HAS_STIBP                  (pf_cpuid_reg(7, 0, 3) >> 27 & 1)
#define PF_HAS_L1D_FLUSH              (pf_cpuid_reg(7, 0, 3) >> 28 & 1)
#define PF_HAS_IA32_ARCH_CAPABILITIES (pf_cpuid_reg(7, 0, 3) >> 29 & 1)
#define PF_HAS_IA32_CORE_CAPABILITIES (pf_cpuid_reg(7, 0, 3) >> 30 & 1)
#define PF_HAS_SSBD                   (pf_cpuid_reg(7, 0, 3) >> 31 & 1)

#endif
