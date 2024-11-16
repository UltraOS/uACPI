// Name: CopyObject to predefined works
// Expect: str => HelloWorld

DefinitionBlock ("x.aml", "SSDT", 1, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (HELO) {
        Return ("Hello")
    }

    Method (WRLD) {
        Return ("World")
    }

    Method (MAIN) {
        OperationRegion(NVSM, SystemMemory, 0x100000, 128)
        Field (NVSM, ByteAcc, Lock, WriteAsZeros) {
            FILD, 8,
        }

        FILD = 0xFF

        CopyObject(HELO, \)
        CopyObject(WRLD, _GL)

        If (FILD != 0xFF) {
            Return ("Locked field read-back failed")
        }

        Return (Concatenate(\(), _GL()))
    }
}
