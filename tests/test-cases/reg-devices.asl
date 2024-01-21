// Name: _REG gets called correctly
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Name (RET, 0)

    Device (PCI0) {
        Name (_HID, "PNP0000")

        Method (_CID) {
            Local0 = Package {
                "idk",
                0x000A2E4F,
                "badid",
                0x02002E4F,
                0x030FD041,
                0x130FD041,
                0x120FD041,

                // PCI Express root bridge
                "PNP0A08"
            }
            Return (Local0)
        }

        Name (_SEG, 0x1)
        Name (_BBN, 0x10)

        Device (HPET) {
            Name (_HID, "PNP0103")

            OperationRegion(LOLX, SystemMemory, 0xDEADBEEF, 123)

            Method (_REG, 2) {
                Debug = "HPET._REG shouldn't have been called"
                RET += 1
            }
        }

        Name (STAT, 0)

        Device (UHCI) {
            Name (_ADR, 0x0001000F)

            Method (TEST, 1, Serialized) {
                Method (_REG, 2, Serialized) {
                    Printf("PCIR _REG(%o, %o) called", Arg0, Arg1)

                    If (Arg0 != 2) {
                        Printf("Invalid space value %o", Arg0)
                        RET += 1
                    }

                    Switch (Arg1) {
                    Case (0) {
                        If (STAT != 1) {
                            Debug = "Trying to _REG(disconnect) before _REG(connect)"
                            RET += 1
                            Break
                        }

                        STAT += 1
                        Break

                    }
                    Case (1) {
                        If (STAT != 0) {
                            Debug = "Trying to run _REG(connect) twice"
                            RET += 1
                            Break
                        }

                        STAT += 1
                        Break
                    }
                    }
                }

                OperationRegion(PCIR, PCI_Config, 0x00, Arg0)
                Field (PCIR, AnyAcc, NoLock) {
                    REG0, 8
                }

                If (STAT != 1) {
                    Debug = "No one ever called _REG on PCIR, giving up"
                    RET += 1
                    Return ()
                }

                REG0 = 123
            }
        }
    }

    Device (PCI1) {
        Name (STAT, 0)

        // PCI root bus
        Name (_HID, "PNP0A03")

        Device (XHCI) {
            Name (_ADR, 0x00030002)

            OperationRegion(HREG, PCI_Config, 0x04, 0xF0)
            Field (HREG, AnyAcc, NoLock) {
                REG2, 8
            }

            // This can only be called after loading the namespace
            Method (_REG, 2) {
                Printf("HREG _REG(%o, %o) called", Arg0, Arg1)

                If (Arg0 != 2) {
                    Printf("Invalid space value %o", Arg0)
                    RET += 1
                }

                If (Arg1 != 1 || STAT != 0) {
                    Printf("Invalid Arg1 (%o) for state %o", Arg1, STAT)
                    RET += 1
                }

                STAT += 1
            }

            Method (WRIT, 1) {
                REG2 = Arg0
                Return (Arg0)
            }
        }

    }

    Method (MAIN) {
        \PCI0.UHCI.TEST(0xFF)
        Local0 = 1

        If (\PCI1.STAT == 1) {
            Local0 -= \PCI1.XHCI.WRIT(1)
        } Else {
            Debug = "PCI1.XHCI._REG was never called!"
        }

        Return (RET + Local0)
    }
}
