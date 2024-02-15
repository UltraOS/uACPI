// Name: Concatenate Resources
// Expect: int => 1

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Name (BUF0, ResourceTemplate ()
    {
        WordBusNumber (ResourceProducer, MinFixed, MaxFixed, PosDecode,
            0x0000,
            0x0000,
            0x00FF,
            0x0000,
            0x0100,
            ,, _Y00)
        DWordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
            0x00000000,
            0x00000000,
            0x00000CF7,
            0x00000000,
            0x00000CF8,
            1, "\\SOME.PATH",, TypeStatic, DenseTranslation)
        IO (Decode16,
            0x0CF8,
            0x0CF8,
            0x01,
            0x08,
            )
        DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
            0x00000000,
            0x000A0000,
            0x000BFFFF,
            0x00000000,
            0x00020000,
            123, "^^^^^^^^^ANOT.ER.PATH", , AddressRangeMemory, TypeStatic)
    })

    Name (IIC0, ResourceTemplate ()
    {
        I2cSerialBusV2 (0x0000, ControllerInitiated, 0x00061A80,
            AddressingMode7Bit, "\\_SB.PCI0.I2C0",
            0x00, ResourceConsumer, _Y10, Exclusive,
        )
    })

    Name (RBUF, ResourceTemplate ()
    {
        I2cSerialBusV2 (0x0029, ControllerInitiated, 0x00061A80,
            AddressingMode7Bit, "\\_SB.PCI0.I2C0",
            0x00, ResourceConsumer, , Exclusive,
            )
        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0x0000,
            "\\_SB.PCI0.GPI0", 0x00, ResourceConsumer, ,
            )
            {
                0x012A
            }
        GpioIo (Exclusive, PullDefault, 0x0000, 0x0000, IoRestrictionOutputOnly,
            "\\_SB.PCI0.GPI0", 0x00, ResourceConsumer, ,
            )
            {
                0x002F
            }
        GpioIo (Exclusive, PullDefault, 0x0000, 0x0000, IoRestrictionOutputOnly,
            "\\_SB.PCI0.GPI0", 0x00, ResourceConsumer, ,
            )
            {
                0x0124
            }
        })

    // src0, src1, dst
    Method (CHEK, 3)
    {
        Local0 = (SizeOf(Arg0) + SizeOf(Arg1)) - 2

        If (Local0 != SizeOf(Arg2)) {
            Printf("Invalid final buffer size: %o, expected %o",
                   Local0, SizeOf(Arg2))
            Return (0)
        }

        Local0 = 0
        Local1 = 0

        While (Local0 < (SizeOf(Arg0) - 2)) {
            Local2 = DerefOf(Arg0[Local0])
            Local3 = DerefOf(Arg2[Local1])

            If (Local2 != Local3) {
                Printf("Byte src=%o (dst=%o) mismatch, expected %o got %o",
                       Local0, Local1, ToHexString(Local2), ToHexString(Local3))
                Return (0)
            }

            Local0 += 1
            Local1 += 1
        }

        Local0 = 0
        While (Local0 < SizeOf(Arg1)) {
            Local2 = DerefOf(Arg1[Local0])
            Local3 = DerefOf(Arg2[Local1])

            If (Local2 != Local3) {
                Printf("Byte src=%o (dst=%o) mismatch, expected %o got %o",
                       Local0, Local1, ToHexString(Local2), ToHexString(Local3))
                Return (0)
            }

            Local0 += 1
            Local1 += 1
        }

        Return (1)
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local0 = ConcatenateResTemplate(BUF0, IIC0)
        If (CHEK(BUF0, IIC0, Local0) != 1) {
            Return (0)
        }

        Local1 = ConcatenateResTemplate(Local0, RBUF)
        Return(CHEK(Local0, RBUF, Local1))
    }
}
