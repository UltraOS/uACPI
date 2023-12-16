// Name: ToX produces correct results
// Expect: int => 0

DefinitionBlock ("", "SSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Name(FCNT, 0)

    Method (CHEK, 2)
    {
        If (ObjectType(Arg0) == 3) {
            Arg0 = ToHexString(Arg0)
        }

        If (ObjectType(Arg1) == 3) {
            Arg1 = ToHexString(Arg1)
        }

        If (Arg0 != Arg1) {
            FCNT++
            Printf("Invalid string %o, expected %o", Arg0, Arg1)
        }
    }

    Method (MAIN, 0, NotSerialized)
    {
        // Dec string
        Local0 = ToDecimalString(123)
        Local1 = "123"
        CHEK(Local0, Local1)

        Local0 = ToDecimalString(Buffer { 1, 2, 222, 33, 45, 192, 3, 255 })
        Local1 = "1,2,222,33,45,192,3,255"
        CHEK(Local0, Local1)

        Local0 = ToDecimalString("")
        Local1 = ""
        CHEK(Local0, Local1)

        Local0 = ToDecimalString("123")
        Local1 = "123"
        CHEK(Local0, Local1)

        Local0 = ToDecimalString(0xFFFFFFFFFFFFFFFF)
        Local1 = "18446744073709551615"
        CHEK(Local0, Local1)

        // Hex string
        Local0 = ToHexString(123)
        Local1 = "0x7B"
        CHEK(Local0, Local1)

        Local0 = ToHexString(Buffer { 1, 2, 222, 33, 45, 192, 3, 255 })
        Local1 = "0x01,0x02,0xDE,0x21,0x2D,0xC0,0x03,0xFF"
        CHEK(Local0, Local1)

        Local0 = ToHexString("")
        Local1 = ""
        CHEK(Local0, Local1)

        Local0 = ToHexString("123")
        Local1 = "123"
        CHEK(Local0, Local1)

        Local0 = ToHexString(0xF)
        Local1 = "0xF"
        CHEK(Local0, Local1)

        Local0 = ToHexString(0xFF)
        Local1 = "0xFF"
        CHEK(Local0, Local1)

        Local0 = ToHexString(0xFFF)
        Local1 = "0xFFF"
        CHEK(Local0, Local1)

        Local0 = ToHexString(0xFFFFF)
        Local1 = "0xFFFFF"
        CHEK(Local0, Local1)

        Local0 = ToHexString(0xFFFFFFFFFFFFFFFF)
        Local1 = "0xFFFFFFFFFFFFFFFF"
        CHEK(Local0, Local1)

        // Buffer
        Local0 = ToBuffer(Buffer { 1, 2, 3 })
        Local1 = Buffer { 1, 2, 3 }
        CHEK(Local0, Local1)

        Local0 = ToBuffer("Hello")
        Local1 = Buffer { 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x00 }
        CHEK(Local0, Local1)

        Local0 = ToBuffer(0xDEADBEEFCAFEBABE)
        Local1 = Buffer { 0xBE, 0xBA, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE }
        CHEK(Local0, Local1)

        Return (FCNT)
    }
}
