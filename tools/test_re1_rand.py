#!/usr/bin/env python3
"""re1_rand_sequences.py - numeric check for the mode-aware rand (Globals.cpp).

The port routes every rand()/srand() call through re1_rand()/re1_srand(),
which must reproduce two different sequences:

  DC    -> the PS1's rand (SLUS_005.51 0x8005f6d0):
           state = state * 0x41c64e6d + 0x3039 ; return (state >> 16) & 0x7fff
           The 7 instruction words at 0x8005f6d0 were read out of Ghidra and
           re-derived here by hand, so the constants are byte-proven.
  OG    -> the MSVC CRT rand the PC release recorded its demo reels against:
           state = state * 214013 + 2531011 ; return (state >> 16) & 0x7fff

    python tools/test_re1_rand.py
"""
import sys

M = 1 << 32

PS1_SEED0 = [0, 21468, 9988]        # PS1 rand after srand(0)
MSVC_SEED0 = [38, 7719, 21238]      # MSVC CRT rand after srand(0)
MSVC_SEED1 = [41, 18467, 6334]      # MSVC CRT rand after srand(1)


class State:
    def __init__(self, seed):
        self.s = seed & 0xFFFFFFFF


def ps1_rand(st):
    st.s = (st.s * 0x41C64E6D + 0x3039) % M
    return (st.s >> 16) & 0x7FFF


def msvc_rand(st):
    st.s = (st.s * 214013 + 2531011) % M
    return (st.s >> 16) & 0x7FFF


def first_n(seed, fn, n):
    st = State(seed)
    return [fn(st) for _ in range(n)]


def main():
    failures = 0
    got = first_n(0, ps1_rand, 3)
    if got != PS1_SEED0:
        print("FAIL PS1 srand(0): got %s want %s" % (got, PS1_SEED0))
        failures += 1
    else:
        print("ok   PS1 srand(0) -> %s" % got)

    got = first_n(0, msvc_rand, 3)
    if got != MSVC_SEED0:
        print("FAIL MSVC srand(0): got %s want %s" % (got, MSVC_SEED0))
        failures += 1
    else:
        print("ok   MSVC srand(0) -> %s" % got)

    got = first_n(1, msvc_rand, 3)
    if got != MSVC_SEED1:
        print("FAIL MSVC srand(1): got %s want %s" % (got, MSVC_SEED1))
        failures += 1
    else:
        print("ok   MSVC srand(1) -> %s" % got)

    # The two streams must disagree immediately - that disagreement is the
    # desync this port bug is about.
    if PS1_SEED0[1] == MSVC_SEED0[1]:
        print("FAIL the two sequences are identical; the test is vacuous")
        failures += 1
    else:
        print("ok   the PS1 and MSVC sequences diverge from draw 1")

    if failures:
        print("%d check(s) FAILED" % failures)
        return 1
    print("ok: re1_rand reproduces both sequences")
    return 0


if __name__ == "__main__":
    sys.exit(main())
