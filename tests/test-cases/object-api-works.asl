// Name: Public API for object mutation works
// Expect: str => check-object-api-works

DefinitionBlock ("x.aml", "SSDT", 1, "uTEST", "APITESTS", 0xF0F0F0F0)
{
    Method (MAIN) {
        // Skip for non-uacpi test runners
        Return ("check-object-api-works")
    }

    /*
     * Arg0 -> Expected case
     * Arg1 -> The actual value
     * Return -> Ones on succeess, Zero on failure
     */
    Method (CHEK, 2) {
        Switch (Arg0) {
        Case (1) {
            Local0 = 0xDEADBEEF
            Break
        }
        Case (2) {
            Local0 = "Hello World"
            Break
        }
        Case (3) {
            Local0 = "TEST"

            /*
             * Arg1 is expected to be a reference to a string XXXX, store
             * into it here to invoke implicit case semantics.
             */
            Arg1 = Local0
            Break
        }
        Case (4) {
            Local0 = Buffer { 0xDE, 0xAD, 0xBE, 0xEF }
            Break
        }
        Case (5) {
            If (ObjectType(Arg1) != 4) {
                Printf("Expected a Package, got %o", Arg1)
                Return (Zero)
            }

            If (SizeOf(Arg1) != 3) {
                Printf("Expected a Package of 3 elements, got %o", SizeOf(Arg1))
                Return (Zero)
            }

            Local0 = Package {
                "First Element",
                2,
                Buffer { 1, 2, 3 },
            }
            Local1 = 0

            While (Local1 < 3) {
                If (DerefOf(Local0[Local1]) != DerefOf(Arg1[Local1])) {
                    Printf("Expected %o, got %o!", DerefOf(Local0[Local1]), DerefOf(Arg1[Local1]))
                    Return (Zero)
                }

                Printf("Object %o OK", Local1)
                Local1++
            }

            Return (Ones)
        }
        }

        If (Local0 != Arg1) {
            Printf("Expected '%o', got '%o'!", Local0, Arg1)
            Return (Zero)
        }

        Printf("Comparison %o OK", Arg0)
        Return (Ones)
    }
}
