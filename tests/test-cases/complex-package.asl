// Name: Nested Package Doesn't Leak Memory
// Expect: int => 1

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method(GPKG) {
        Local1 = 10
        Local0 = Package (Local1) {
            0x123,
            0x321,
            Package {
                0x321,
                "123",
                Package {
                    0x321,
                    Package {
                        0x321,
                        "123",
                        Package {
                            0x321,
                            "123",
                                Package {
                                    0x321,
                                    Package {
                                        0x321,
                                        "123",
                                        Package (Local1) {
                                            0x321,
                                            "123",
                                            999,
                                        },
                                        999,
                                    },
                                "123",
                                999,
                            },
                            999,
                        },
                        999,
                    },
                    "123",
                    999,
                },
                999,
            },
            "Hello world",
            Package {
                0x321,
                "Hello",
            },
            Package {
                0x321,
                "World",
            },
            Package {
                Buffer (Local1) { 0xFF },
                0xDEADBEEF,
            },
            Buffer { 1, 2, 3 }
        }

        Return (Local0)
    }

    Method (MAIN) {
        Local0 = GPKG()
        Debug = Local0
        Local0 = 1
        Return (Local0)
    }
}
