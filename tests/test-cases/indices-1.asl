// Name: Dump Package Contents
// Expect: str => { HelloWorld, 123, deadbeef, { some string, { ffffffffeeeeeeee, middle package }, cafebabe } }

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (DUMP, 1)
    {
        Local0 = 0
        Local1 = "{ "

        While (Local0 < SizeOf(Arg0)) {
            // If package, invoke DUMP recursively
            If (ObjectType(DerefOf(Arg0[Local0])) == 4) {
                Fprintf(Local1, "%o, %o", Local1, DUMP(DerefOf(Arg0[Local0])))
                Local0 += 1
                Continue;
            }

            If (Local0 == 0) {
                Local3 = ""
            } Else {
                Local3 = ", "
            }

            Fprintf(Local1, "%o%o%o", Local1, Local3, DerefOf(Arg0[Local0]))
            Local0 += 1
        }

        Fprintf(Local1, "%o }", Local1)
        Return(Local1)
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local0 = Package {
            "HelloWorld",
            0x123,
            0xDEADBEEF,
            Package {
                "some string",
                Package {
                    0xFFFFFFFFEEEEEEEE,
                    "middle package",
                },
                0xCAFEBABE,
            },
        }

        Local1 = DUMP(Local0)
        Return(Local1)
    }
}
