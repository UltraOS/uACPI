// Name: Eval supports plain objects
// Expect: str => This is a plain string, not a method

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Name (MAIN, "This is a plain string, not a method")
}
