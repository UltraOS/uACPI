// Name: Scopes and undefined references
// Expect: int => 13

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Name (MAIN, 0)

    Scope (\PATH.THAT.DOES.NOT.EXIS.T) {
        Debug = "Why are we here"
        MAIN += 1
    }

    Scope (\_SB) {
        MAIN += 3

        Scope (^ANOT.HER) {
            MAIN += 4
        }

        Scope (\_GPE) {
            MAIN += 10

            Scope (PATH) {
                MAIN += 200
            }
        }

        Scope (FAIL.TOO) {
            MAIN += 300
        }
    }
}
