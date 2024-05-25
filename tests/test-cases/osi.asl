// Name: _OSI works correctly
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (CHEK, 2) {
        Local0 = _OSI(Arg0)

        If (Local0 != Arg1) {
            Printf("_OSI(%o) failed (expected %o, got %o)", Arg0, Arg1, Local0)
            Return (1)
        }

        Return (0)
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local0 = 0

        If (!CondRefOf(_OSI)) {
            Debug = "No _OSI method!"
            Return (1111111)
        }

        Local0 += CHEK("Windows 2000", Ones)
        Local0 += CHEK("Windows 2001", Ones)
        Local0 += CHEK("Windows 2001 SP1", Ones)
        Local0 += CHEK("Windows 2001.1", Ones)
        Local0 += CHEK("Windows 2001 SP2", Ones)
        Local0 += CHEK("Windows 2001.1 SP1", Ones)
        Local0 += CHEK("Windows 2006.1", Ones)
        Local0 += CHEK("Windows 2006 SP1", Ones)
        Local0 += CHEK("Windows 2006 SP2", Ones)
        Local0 += CHEK("Windows 2009", Ones)
        Local0 += CHEK("Windows 2012", Ones)
        Local0 += CHEK("Windows 2013", Ones)
        Local0 += CHEK("Windows 2015", Ones)
        Local0 += CHEK("Windows 2016", Ones)
        Local0 += CHEK("Windows 2017", Ones)
        Local0 += CHEK("Windows 2017.2", Ones)
        Local0 += CHEK("Windows 2018", Ones)
        Local0 += CHEK("Windows 2018.2", Ones)
        Local0 += CHEK("Windows 2019", Ones)

        // ACPICA acpiexec
        If (_OSI("AnotherTestString")) {
            // do nothing
            Debug = "ACPICA acpiexec detected"
        } ElseIf (_OSI("TestRunner")) {
            Debug = "uACPI test runner detected"

            // These are only enabled in uACPI test runner
            Local0 += CHEK("3.0 Thermal Model", Ones)
            Local0 += CHEK("Module Device", Ones)

            // Don't check these in ACPICA, it might be too old to have these
            Local0 += CHEK("Windows 2020", Ones)
            Local0 += CHEK("Windows 2021", Ones)
            Local0 += CHEK("Windows 2022", Ones)
        } Else {
            Debug = "Neither uACPI nor ACPICA were detected, aborting test"
            Return (123321)
        }

        // This is removed in both uACPI and ACPICA test runners
        Local0 += CHEK("Windows 2006", Zero)

        Local0 += CHEK("Extended Address Space Descriptor", Ones)

        Local0 += CHEK("Processor Aggregator Device", Zero)
        Local0 += CHEK("3.0 _SCP Extensions", Zero)
        Local0 += CHEK("Processor Device", Zero)
        Local0 += CHEK("", Zero)
        Local0 += CHEK("Windows 99999", Zero)
        Local0 += CHEK("Windows 2014", Zero)
        Local0 += CHEK("Linux", Zero)

        Return (Local0)
    }
}
