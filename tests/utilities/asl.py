import os
import subprocess
from typing import List, Optional, Union


class ASL:
    @staticmethod
    def definition_block(kind: str, revision: int) -> str:
        block = "DefinitionBlock "
        block += f'("", "{kind}", {revision}, '
        block += '"uTEST", "TESTTABL", 0xF0F0F0F0)'

        return block

    @staticmethod
    def name(name: str, term_arg: Union[str, int]) -> str:
        return f"Name({name}, {term_arg})"

    @staticmethod
    def buffer(
        initializers: Optional[List[int]] = None,
        count: Optional[int] = None
    ) -> str:
        if count is not None:
            buf_len = count
        elif initializers is not None:
            buf_len = len(initializers)
        else:
            buf_len = 0

        buf = f"Buffer({buf_len})"

        buf += " { "
        if initializers is not None:
            buf += ", ".join([str(x) for x in initializers])
        buf += " }"

        return buf

    @staticmethod
    def method(name: str, num_args: Optional[int] = None) -> str:
        method = f"Method ({name}"
        if num_args is not None:
            method += f", {num_args}"
        method += ")"

        return method

    @staticmethod
    def invoke(name: str, args: List[str] = []) -> str:
        invocation = f"{name}("
        invocation += ", ".join(args)
        invocation += ")"
        return invocation

    @staticmethod
    def end_block() -> str:
        return "}"

    @staticmethod
    def assign(dst: str, src: str) -> str:
        return f"{dst} = {src}"

    @staticmethod
    def increment(name: str) -> str:
        return f"{name}++"

    @staticmethod
    def create_field(buf: str, idx: int, field_size: int,
                     field_name: str) -> str:
        return f"CreateField({buf}, {idx}, {field_size}, {field_name})"

    @staticmethod
    def iff(arg: str) -> str:
        return f"If ({arg})"

    @staticmethod
    def elsee() -> str:
        return "Else"

    @staticmethod
    def equal(arg0: str, arg1: str) -> str:
        return f"{arg0} == {arg1}"

    @staticmethod
    def returnn(arg: str) -> str:
        return f"Return ({arg})"


class ASLSource:
    def __init__(self, revision: int = 0):
        self.lines: List[str] = []
        self.revision = revision
        self.indentation = 0

        self.l(ASL.definition_block("DSDT", 2))
        self.block_begin()

    def l(self, line: str) -> None:
        line = " " * (self.indentation * 4) + line
        self.lines.append(line)

    def block_begin(self) -> None:
        self.l("{")
        self.indentation += 1

    def block_end(self) -> None:
        self.indentation -= 1
        self.l("}\n")

    def iff(self, arg: str) -> None:
        self.l(ASL.iff(arg) + " {")
        self.indentation += 1

    def elsee(self) -> None:
        self.indentation -= 1
        self.l("} Else {")
        self.indentation += 1

    def finalize(self) -> None:
        self.block_end()

    def dump(self, path: str) -> None:
        with open(path, "w") as f:
            f.write("\n".join(self.lines))

    def dump_as_test_case(
        self, path: str, case_name: str,
        expected_type: str, expected_value: Union[str, int]
    ) -> None:
        with open(path, "w") as f:
            f.write(f"// Name: (generated) {case_name}\n")
            f.write(f"// Expect: {expected_type} => {expected_value}\n")
            f.write("\n".join(self.lines))

    @staticmethod
    def compile(path: str, compiler: str, bin_dir: str) -> str:
        case_aml_name = os.path.basename(path).rsplit(".", 1)[0] + ".aml"
        out_case = os.path.join(bin_dir, case_aml_name)

        ignored_warnings = [
            # Warning 3144 Method Local is set but never used
            "-vw", "3144",
            # Remark 2098 Recursive method call
            "-vw", "2098",
        ]

        args = [compiler, *ignored_warnings, "-p", out_case, path]
        proc = subprocess.Popen(args, stdout=subprocess.PIPE,
                                universal_newlines=True)

        proc.wait(10)
        stdout = proc.stdout
        assert stdout

        if proc.returncode != 0:
            raise RuntimeError(f"Compiler error: {stdout.read()}")

        return out_case
