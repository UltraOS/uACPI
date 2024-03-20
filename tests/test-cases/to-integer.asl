// Name: ToInteger produces correct results
// Expect: int => 0

DefinitionBlock ("", "SSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Name(FCNT, 0)

    Method (CHEK, 3)
    {
        If (Arg0 != Arg1) {
            FCNT++
            Printf("On line %o: invalid number %o, expected %o",
                   ToDecimalString(Arg2), ToHexString(Arg0), ToHexString(Arg1))
        }
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local0 = ToInteger(123)
        Local1 = 123
        CHEK(Local0, Local1, __LINE__)

        Local0 = ToInteger("123")
        Local1 = 123
        CHEK(Local0, Local1, __LINE__)

        Local0 = ToInteger("       \t\t\t\v       123")
        Local1 = 123
        CHEK(Local0, Local1, __LINE__)

        Local0 = ToInteger("123abcd")
        Local1 = 123
        CHEK(Local0, Local1, __LINE__)

        Local0 = ToInteger("0x123abcd")
        Local1 = 0x123abcd
        CHEK(Local0, Local1, __LINE__)

        Local0 = ToInteger("")
        Local1 = 0
        CHEK(Local0, Local1, __LINE__)

        Local0 = ToInteger("0X")
        Local1 = 0
        CHEK(Local0, Local1, __LINE__)

        Local0 = ToInteger("0x")
        Local1 = 0
        CHEK(Local0, Local1, __LINE__)

        Local0 = ToInteger("0")
        Local1 = 0
        CHEK(Local0, Local1, __LINE__)

        Local0 = ToInteger("0xDeAdBeeF")
        Local1 = 0xDEADBEEF
        CHEK(Local0, Local1, __LINE__)

        Local0 = ToInteger("0XDeAdBeeFCafeBabeHelloWorld")
        Local1 = 0xDEADBEEFCAFEBABE
        CHEK(Local0, Local1, __LINE__)

        Local0 = ToInteger(Buffer { 0xDE, 0xAD, 0xBE, 0xEF })
        Local1 = 0xEFBEADDE
        CHEK(Local0, Local1, __LINE__)

        Local0 = ToInteger(Buffer { 1 })
        Local1 = 1
        CHEK(Local0, Local1, __LINE__)

        Local0 = ToInteger(Buffer { 0 })
        Local1 = 0
        CHEK(Local0, Local1, __LINE__)


        Local0 = ToInteger(Buffer { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE })
        Local1 = 0xBEBAFECAEFBEADDE
        CHEK(Local0, Local1, __LINE__)

        // These are incompatible with ACPICA, skip if it's detected
        If (ToHexString(0xF) == "0xF") {
            Local0 = ToInteger("99999999999999999999999999999999999999999999999")
            Local1 = 0xFFFFFFFFFFFFFFFF
            CHEK(Local0, Local1, __LINE__)

            Local0 = ToInteger("0xDEADBEEFCAFEBABE1")
            Local1 = 0xFFFFFFFFFFFFFFFF
            CHEK(Local0, Local1, __LINE__)

            Local0 = ToInteger("+123")
            Local1 = 123
            CHEK(Local0, Local1, __LINE__)

            Local0 = ToInteger("-123")
            Local1 = 0xFFFFFFFFFFFFFF85
            CHEK(Local0, Local1, __LINE__)

            Local0 = ToInteger("-0xDEADBEF HELLOWORLD")
            Local1 = 0xFFFFFFFFF2152411
            CHEK(Local0, Local1, __LINE__)

            Local0 = ToInteger("+0XC0D\t123")
            Local1 = 0xC0D
            CHEK(Local0, Local1, __LINE__)

            Local0 = ToInteger("0123")
            Local1 = 83
            CHEK(Local0, Local1, __LINE__)
        }

        Return (FCNT)
    }
}
