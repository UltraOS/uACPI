// Name: Duplicate named objects are skipped correctly
// Expect: int => 11

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Name (MAIN, 0)

    Name (TEST, "Hello World")
    Device (FOO) {
        ThermalZone (BAR) {
            Name (TEST, "Hello World")
        }
    }

    // These all attempt to create duplicate objects
    Name (FOO.BAR.TEST, "duplicated test")
    MAIN += 1
    Method (TEST, 0, Serialized) {
        Debug = "Why is this executed?"
        Name (TEST, 123)
        CopyObject("Method didn't get skipped", MAIN)
        Return (333)
    }

    Debug = "Ok, still here"
    MAIN += 1

    ThermalZone (TEST) {
        Local0 = 123
        Debug = Local0
        CopyObject("???", MAIN)
    }
    ThermalZone (TEST) { }
    MAIN += 1

    Processor (FOO.BAR.TEST, 0x02, 0x00000410, 0x06) { }
    MAIN += 1
    Processor (\TEST, 0x01, 0x00000410, 0x06) {
        Local2 = Package { 1, 2, 3 }
        Debug = Local2
    }

    Device (\FOO.BAR.TEST)
    {
        Name (_HID, EisaId ("PNP0C0D"))
    }

    MAIN += 1
    Device (\TEST) { }

    // Alias to an object that doesn't exist, but new name is valid
    Alias(ZOO, BAR)
    Alias(PATH.THAT.DOES.NOT.EXIS.T, \BAZ)
    MAIN += 1
    // Alias to an object that does exist, but new name alrady exists
    Alias(\TEST, \MAIN)

    // Alias to a non existant object and name also already exists
    Alias(ZOO, \TEST)
    Alias(PATH.THAT.DOES.NOT.EXIS.T, \FOO.BAR.TEST)

    MAIN += 1
    Mutex(TEST, 15)

    Debug = "Just a bit left"

    Event(TEST)
    MAIN += 1
    OperationRegion(TEST, SystemMemory, 0x100000, 128)
    DataTableRegion(FOO.BAR.TEST, "DSDT", "", "")

    Local0 = Buffer (256) { }

    CreateBitField(Local0, 111, TEST)
    CreateByteField(Local0, 111, TEST)
    MAIN += 1
    CreateDWordField(Local0, 111, TEST)
    CreateQWordField(Local0, 111, TEST)
    MAIN += 1
    CreateField(Local0, 111, 11, FOO.BAR.TEST)

    MAIN += 1
    Debug = "Made it to the end!"
}
