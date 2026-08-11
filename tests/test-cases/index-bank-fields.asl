// Name: Index & bank fields work
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "IDXBNKFD", 0xF0F0F0F0)
{
    Name (FAIL, 0)

    Method (ERR, 2) {
        Printf("%o check failed, unexpected value %o", Arg0, ToHexString(Arg1))
        FAIL++
    }

    OperationRegion (SYSM, SystemMemory, 0x10000, 0x100)

    // A direct view of the emulated index/data & bank register pairs
    Field (SYSM, ByteAcc, NoLock, Preserve) {
        IDXR, 8,
        DATR, 8,
        Offset (0x20),
        BNKR, 8,
        BDAT, 8,
    }

    // A separate register pair that requires the global lock
    Field (SYSM, ByteAcc, Lock, Preserve) {
        Offset (2),
        LIDX, 8,
        LDAT, 8,
    }

    IndexField (IDXR, DATR, ByteAcc, NoLock, Preserve) {
        Offset (0x10),
        IF0, 8,
        IF1, 8,
        , 3,
        IF2, 6,
    }

    // The lock is held across the entire transaction here
    IndexField (IDXR, DATR, ByteAcc, Lock, Preserve) {
        Offset (0x30),
        LIF0, 16,
    }

    // NoLock itself, but the underlying register pair is Lock
    IndexField (LIDX, LDAT, ByteAcc, NoLock, Preserve) {
        Offset (4),
        LIF1, 8,
    }

    BankField (SYSM, BNKR, 0xA5, ByteAcc, NoLock, Preserve) {
        Offset (0x21),
        BFA, 8,
    }

    BankField (SYSM, BNKR, 0x5A, ByteAcc, Lock, Preserve) {
        Offset (0x21),
        BFB, 8,
    }

    Method (TIDX) {
        IF0 = 0xAA
        If (IDXR != 0x10) { ERR("IF0 index", IDXR) }
        If (DATR != 0xAA) { ERR("IF0 data", DATR) }

        /*
         * The emulated data register simply remembers the last written
         * value, expect it to be returned for every datum read.
         */
        Local0 = IF1
        If (IDXR != 0x11) { ERR("IF1 index", IDXR) }
        If (Local0 != 0xAA) { ERR("IF1 value", Local0) }

        /*
         * A misaligned field spanning two datums, each of which is
         * read-modify-written on top of the last data register value:
         * bits [3:7] of 0x2D go into 0xAA, making 0x6A, then bit [0:0]
         * into 0x6A, making 0x6B.
         */
        IF2 = 0x2D
        If (IDXR != 0x13) { ERR("IF2 index", IDXR) }
        If (DATR != 0x6B) { ERR("IF2 data", DATR) }

        // Both datums are extracted from the same leftover 0x6B
        Local0 = IF2
        If (Local0 != 0x2D) { ERR("IF2 value", Local0) }

        LIF0 = 0x1234
        If (IDXR != 0x31) { ERR("LIF0 index", IDXR) }
        If (DATR != 0x12) { ERR("LIF0 data", DATR) }

        LIF1 = 0x77
        If (LIDX != 4) { ERR("LIF1 index", LIDX) }
        If (LDAT != 0x77) { ERR("LIF1 data", LDAT) }
    }

    Method (TBNK) {
        BFA = 0x11
        If (BNKR != 0xA5) { ERR("BFA bank", BNKR) }
        If (BDAT != 0x11) { ERR("BFA data", BDAT) }

        // The same data byte through the other bank, with the lock held
        Local0 = BFB
        If (BNKR != 0x5A) { ERR("BFB bank", BNKR) }
        If (Local0 != 0x11) { ERR("BFB value", Local0) }
    }

    Method (MAIN) {
        TIDX()
        TBNK()
        Return (FAIL)
    }
}
