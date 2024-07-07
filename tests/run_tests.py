#!/usr/bin/python3
import subprocess
import argparse
import os
import sys
import platform
from typing import List, Tuple, Optional
from types import TracebackType
from abc import ABC, abstractmethod

from utilities.asl import ASLSource
import generated_test_cases.buffer_field as bf


def abs_path_to_current_dir() -> str:
    return os.path.dirname(os.path.abspath(__file__))


def generate_test_cases(compiler: str, bin_dir: str) -> List[str]:
    return [
        bf.generate_buffer_reads_test(compiler, bin_dir),
        bf.generate_buffer_writes_test(compiler, bin_dir),
    ]


ACPI_DUMPS_URL = "https://github.com/UltraOS/ACPIDumps.git"


class TestCase(ABC):
    def __init__(self, path: str, name: str):
        self.path = path
        self.name = name

    @abstractmethod
    def extra_runner_args(self) -> List[str]:
        pass


class TestCaseWithMain(TestCase):
    def __init__(
        self, path: str, name: str, rtype: str, value: str
    ) -> None:
        super().__init__(path, f"{os.path.basename(path)}:{name}")
        self.rtype = rtype
        self.value = value

    def extra_runner_args(self) -> List[str]:
        return ["--expect", self.rtype, self.value]


class TestCaseHardwareBlob(TestCase):
    def __init__(self, path: str) -> None:
        dsdt_path = os.path.join(path, "dsdt.dat")
        super().__init__(dsdt_path, os.path.basename(path))

        self.ssdt_paths = [
            path for path in os.listdir(path)
            if path.startswith("ssdt") and path.endswith(".dat")
        ]

        def extract_ssdt_number(path: str) -> int:
            number = ""

            assert path.startswith("ssdt")
            for c in path[4:]:
                if not c.isdigit():
                    break
                number += c

            # some blobs apparently come with just "ssdt.dat" and not
            # "ssdtX.dat", take that into account here.
            return 0 if not number else int(number)

        if self.ssdt_paths:
            self.ssdt_paths.sort(key=extract_ssdt_number)
            self.ssdt_paths = [
                os.path.join(path, ssdt_path) for ssdt_path in self.ssdt_paths
            ]

    def extra_runner_args(self) -> List[str]:
        args = ["--enumerate-namespace"]

        if self.ssdt_paths:
            args.append("--extra-tables")
            args.extend(self.ssdt_paths)

        return args


def generate_large_test_cases(extractor: str, bin_dir: str) -> List[TestCase]:
    acpi_dumps_dir = os.path.join(abs_path_to_current_dir(), "acpi-dumps")
    large_tests_dir = os.path.join(bin_dir, "large-tests")

    if not os.path.exists(acpi_dumps_dir):
        subprocess.check_call(["git", "clone", ACPI_DUMPS_URL, acpi_dumps_dir])

    os.makedirs(large_tests_dir, exist_ok=True)
    test_cases = []

    def recurse_one(path, depth=1):
        for obj in os.listdir(path):
            if obj.startswith("."):
                continue

            obj_path = os.path.join(path, obj)

            if os.path.isdir(obj_path):
                recurse_one(obj_path, depth + 1)
                continue

            if depth == 1 or not obj.endswith(".bin"):
                continue

            print(f"Preparing HW blob {obj_path}...")

            split_path = obj_path.split(os.path.sep)[-depth:]
            fixed_up_path = [
                seg.replace(" ", "_").lower() for seg in split_path
            ]

            test_case_name = "_".join(fixed_up_path).replace(".bin", "")
            this_test_dir = os.path.join(large_tests_dir, test_case_name)

            if (not os.path.exists(this_test_dir) or not
               os.path.exists(os.path.join(this_test_dir, "dsdt.dat"))):
                os.makedirs(this_test_dir, exist_ok=True)

                # These are two separate invocations because of a bug in
                # acpixtract where it exits with -1 when there isn't an SSDT
                # inside a blob, even though it's specified as optional in
                # code. Merge once https://github.com/acpica/acpica/pull/959
                # is shipped everywhere.
                subprocess.check_call(
                    [extractor, "-sDSDT", obj_path], cwd=this_test_dir,
                    stdout=subprocess.DEVNULL
                )
                subprocess.run(
                    [extractor, "-sSSDT", obj_path], cwd=this_test_dir,
                    stdout=subprocess.DEVNULL
                )

            test_cases.append(TestCaseHardwareBlob(this_test_dir))

    recurse_one(acpi_dumps_dir)
    return test_cases


def get_case_name_and_expected_result(case: str) -> Tuple[str, str, str]:
    with open(case) as tc:
        name = tc.readline()
        name = name[name.find(":") + 1:].strip()

        expected_line = tc.readline()
        expected_line = expected_line[expected_line.find(":") + 1:].strip()
        expected = [val.strip() for val in expected_line.split("=>")]

        return name, expected[0], expected[1]


class TestHeaderFooter:
    def __init__(self, text: str) -> None:
        self.hdr = "{:=^80}".format(" " + text + " ")

    def __enter__(self) -> None:
        print(self.hdr, flush=True)

    def __exit__(
        self, exc_type: Optional[type[BaseException]],
        ex: Optional[BaseException], traceback: Optional[TracebackType]
    ) -> Optional[bool]:
        print("=" * len(self.hdr), flush=True)
        return None


def run_resource_tests(runner: str) -> int:
    with TestHeaderFooter("Resource Conversion Tests"):
        return subprocess.run([runner, "resource-tests"]).returncode


def compile_test_cases(
    test_cases: List[str], compiler: str, bin_dir: str
) -> List[TestCase]:
    compiled_cases: List[TestCase] = []

    for case in test_cases:
        print(f"Compiling {case}...", end="")

        # Skip the table loading test for old iASL, it prints bogus error
        # messages and refuses to compile the test case no matter what I try:
        #
        #                             If (!Load(TABL)) {
        # Error    6126 -       syntax error ^
        #
        if os.path.basename(case) == "table-loading-0.asl":
            out = subprocess.check_output([compiler, "-v"],
                                          universal_newlines=True)
            # I don't know which versions it's broken for specifically, this
            # one comes with Ubuntu 22.04, so hardcode it.
            if "20200925" in out:
                print("SKIPPED (bugged iASL)", flush=True)
                continue

        compiled_cases.append(
            TestCaseWithMain(
                ASLSource.compile(case, compiler, bin_dir),
                *get_case_name_and_expected_result(case)
            )
        )
        print("")

    return compiled_cases


def run_tests(cases: List[TestCase], runner: str) -> bool:
    fail_count = 0

    for case in cases:
        print(f"{case.name}...", end=" ", flush=True)

        proc = subprocess.Popen(
            [runner, case.path, *case.extra_runner_args()],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            universal_newlines=True
        )
        try:
            stdout, stderr = proc.communicate(timeout=60)
            if proc.returncode == 0:
                print("OK", flush=True)
                continue
        except subprocess.TimeoutExpired:
            print("TIMEOUT", flush=True)
            proc.kill()
            stdout, stderr = proc.communicate()
        else:
            print("FAIL", flush=True)

        fail_count += 1
        output = ""

        def format_output(source: str, data: Optional[str]) -> str:
            output = ""

            if not data:
                return output

            output += f"\t{source}:\n"
            output += "\n".join(["\t" + line for line in data.split("\n")])

            return output

        output += format_output("stdout", stdout)
        output += format_output("stderr", stderr)

        if output:
            print("TEST OUTPUT:", flush=True)
            print(output, flush=True)
        else:
            print("NO OUTPUT FROM TEST", flush=True)

    pass_count = len(cases) - fail_count
    print(f"SUMMARY: {pass_count}/{len(cases)}", end="")

    if fail_count:
        print(f" ({fail_count} FAILED)")
    else:
        print(" (ALL PASS!)")

    return not fail_count


def test_relpath(*args: str) -> str:
    return os.path.join(abs_path_to_current_dir(), *args)


def test_runner_binary() -> str:
    out = "test-runner"

    if platform.system() == "Windows":
        out += ".exe"

    return out


def build_test_runner(bitness: int) -> str:
    build_dir = f"build-{platform.system().lower()}-{bitness}bits"
    runner_build_dir = test_relpath("runner", build_dir)
    runner_exe = os.path.join(runner_build_dir, test_runner_binary())

    cmake_args: List[str] = ["cmake", ".."]
    if bitness == 32:
        if platform.system() == "Windows":
            cmake_args.extend(["-A", "Win32"])
        else:
            cmake_args.extend([
                "-DCMAKE_CXX_FLAGS=-m32",
                "-DCMAKE_C_FLAGS=-m32"
            ])

    if not os.path.isdir(runner_build_dir):
        os.makedirs(runner_build_dir, exist_ok=True)
        subprocess.run(cmake_args, cwd=runner_build_dir, check=True)

    subprocess.run(["cmake", "--build", "."], cwd=runner_build_dir, check=True)
    return runner_exe


def main() -> int:
    parser = argparse.ArgumentParser(description="Run uACPI tests")
    parser.add_argument("--asl-compiler",
                        help="Compiler to use to build test cases",
                        default="iasl")
    parser.add_argument("--acpi-extractor",
                        help="ACPI extractor utility to use for ACPI dumps",
                        default="acpixtract")
    parser.add_argument("--test-dir",
                        default=test_relpath("test-cases"),
                        help="The directory to run tests from, defaults to "
                             "'test-cases' in the same directory")
    parser.add_argument("--test-runner",
                        help="The test runner binary to invoke")
    parser.add_argument("--binary-directory",
                        default=test_relpath("bin"),
                        help="The directory to store intermediate files in, "
                             "created & deleted automatically. Defaults to "
                             "'bin' in the same directory")
    parser.add_argument("--bitness", default=64, choices=[32, 64], type=int,
                        help="uACPI build bitness")
    parser.add_argument("--large", action="store_true",
                        help="Run the large test suite as well")
    args = parser.parse_args()

    test_compiler = args.asl_compiler
    test_dir = args.test_dir
    test_runner = args.test_runner
    if test_runner is None:
        test_runner = build_test_runner(args.bitness)

    ret = run_resource_tests(test_runner)
    if ret != 0:
        sys.exit(ret)

    bin_dir = args.binary_directory
    os.makedirs(bin_dir, exist_ok=True)

    test_cases = [
        os.path.join(test_dir, f)
        for f in os.listdir(test_dir)
        if os.path.splitext(f)[1] == ".asl"
    ]
    test_cases.extend(generate_test_cases(test_compiler, bin_dir))

    base_test_cases = compile_test_cases(
        test_cases, test_compiler, bin_dir
    )
    with TestHeaderFooter("AML Tests"):
        ret = run_tests(base_test_cases, test_runner)

    if ret and args.large:
        large_test_cases = generate_large_test_cases(
            args.acpi_extractor, bin_dir
        )

        with TestHeaderFooter("Large AML Tests"):
            ret = run_tests(large_test_cases, test_runner)

    sys.exit(not ret)


if __name__ == "__main__":
    main()
