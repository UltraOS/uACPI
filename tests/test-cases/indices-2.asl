// Name: Package Indices w/ References
// Expect: int => 1062815831

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (TEST, 1)
    {
        Local0 = Package {
            1, 2, 3, "String"
        }
        Local1 = 0

        Arg0[1] = RefOf(Local0)
        Arg0[2] = RefOf(Local1)
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local0 = Package {
            "HelloWorld",
            0x123,
            0xDEADBEEF,
            Package {
                "some string",
                0xCAFEBABE
            }
        }

        TEST(Local0)

        Local1 = DerefOf(DerefOf(Local0[1])[3])
        If (Local1 != "String") {
            Printf("Invalid value at nested package %o", Local1)
            Return (1)
        }

        Local0[2] = "WHY?"
        Local2 = DerefOf(Local0[2])

        // Why in little-endian ascii
        Local3 = 1062815831

        If (Local2 != Local3) {
            Printf("Failed to implicit cast, expected %o, got %o", Local3, Local2)
            Return (1)
        }

        Return (Local2)
    }
}
