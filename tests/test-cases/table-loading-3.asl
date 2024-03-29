// Name: Recursive table loads with LoadTable
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    External(ITEM, IntObj)
    External(NUMB, IntObj)
    External(NUMA, IntObj)
    External(VISI, IntObj)

    // All dynamic loads branch into here
    If (CondRefOf(ITEM)) {
        // Recursively start 10 table loads
        If (NUMB < 10) {
            NUMB += 1
            Local0 = NUMB

            If (NUMB == 3) {
                Local0 = LoadTable("DSDT", "uTEST", "", "", "PARA", Local0)
                VISI += 1
            } ElseIf (NUMB == 5) {
                Local0 = LoadTable("DSDT", "", "", "", "PARA", Local0)
                VISI += 10
            } ElseIf (NUMB == 7) {
                Local0 = LoadTable("DSDT", "", "TESTTABL", "", "", Local0)
                VISI += 100
            } Else {
                Local0 = LoadTable("DSDT", "uTEST", "TESTTABL", "", "PARA", Local0)
                VISI += 1000
            }

            // The last load is expected to fail, everything before should succeed
            If (!Local0) {
                If (NUMB != 10) {
                    NUMA = 0xDEADBEEF
                    Printf("Table load %o failed", NUMB)
                }
            } Else {
                NUMA += 1
            }

            // Return something bogus here to make sure the return value isn't
            // propagated to the caller of Load.
            Return (Package { 1, 2 ,3})
        }

        // We're the last table load, do something naughty to cause an error
        Local0 = Package { 1 }
        Local1 = RefOf(Local0)

        // This code specifically attempts to perform a bogus implicit cast
        Local1 = "Hello World"
    }

    Name (ITEM, 123)

    // Number of started table loads
    Name (NUMB, 0)

    // Number of finished table loads
    Name (NUMA, 0)

    // Visited branches
    Name (VISI, 0)

    Name (PARA, 0)
    Name (PASS, "FAIL")

    Method (MAIN, 0, Serialized)
    {
        Local0 = LoadTable("DSDT", "uTEST", "TESTTABL", "", "PASS", 0x53534150)
        Printf("Recursive loads finished!")

        If (!Local0) {
            Printf("Table load failed!")
            Return (0xCAFEBABE)
        }

        If (NUMB != 10) {
            Printf("Invalid NUMB value %o", ToDecimalString(NUMB))
            Return (0xEEFFAABB)
        }

        If (VISI != 7111) {
            Printf("Invalid VISI value %o", ToDecimalString(VISI))
            Return (0xAFFAAFFA)
        }

        If (NUMA != 9) {
            Printf("Invalid NUMA value %o", ToDecimalString(NUMA))
            Return (0x11223344)
        }

        If (PARA != 1) {
            Printf("Invalid PARA value %o", ToDecimalString(PARA))
            Return (0xDDFFBBCC)
        }

        If (PASS != "PASS") {
            Printf("Invalid PASS value %o", PASS)
            Return (0xECECECEC)
        }

        Return (0)
    }
}
