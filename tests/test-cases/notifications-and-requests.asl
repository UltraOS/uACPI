// Name: Notifications & Requests
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (FATL, 1)
    {
        Fatal(0xFF, 0xDEADBEEF, ToInteger(Arg0))
    }

    Device (PSP) {
        Name (_HID, "ACPI0000")
    }

    Method (MAIN, 0, NotSerialized)
    {
        Breakpoint
        FATL("0xCAFEBABEC0DEDEAD")
        Breakpoint

        Notify(PSP, 0x10)

        Return (0)
    }
}
