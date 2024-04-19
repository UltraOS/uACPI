#!/usr/bin/python3
import subprocess
import argparse
import os
import sys
import platform
from typing import List, Tuple, Optional
from types import TracebackType

from utilities.asl import ASLSource
import generated_test_cases.buffer_field as bf


def abs_path_to_current_dir() -> str:
    return os.path.dirname(os.path.abspath(__file__))


def generate_test_cases(compiler: str, bin_dir: str) -> List[str]:
    return [
        bf.generate_buffer_reads_test(compiler, bin_dir),
        bf.generate_buffer_writes_test(compiler, bin_dir),
    ]


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
        return subprocess.run([runner, "--test-resources"]).returncode


def run_tests(
    cases: List[str], runner: str, compiler: str,
    bin_dir: str
) -> int:
    fail_count = 0
    skipped_count = 0

    for case in cases:
        name, rtype, value = get_case_name_and_expected_result(case)
        case_name = os.path.basename(case)

        print(f"{case_name}:{name}...", end=" ", flush=True)

        # Skip the table loading test for old iASL, it prints bogus error
        # messages and refuses to compile the test case no matter what I try:
        #
        #                             If (!Load(TABL)) {
        # Error    6126 -       syntax error ^
        #
        if case_name == "table-loading-0.asl":
            out = subprocess.check_output([compiler, "-v"],
                                          universal_newlines=True)
            # I don't know which versions it's broken for specifically, this
            # one comes with Ubuntu 22.04, so hardcode it.
            if "20200925" in out:
                skipped_count += 1
                print("SKIPPED (bugged iASL)", flush=True)
                continue

        compiled_case = ASLSource.compile(case, compiler, bin_dir)
        proc = subprocess.Popen([runner, compiled_case, rtype, value],
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,
                                universal_newlines=True)
        try:
            stdout, stderr = proc.communicate(timeout=10)
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

    skipped_str = f", {skipped_count} SKIPPED" if skipped_count else ""
    pass_count = len(cases) - fail_count - skipped_count
    print(
        f"SUMMARY: {pass_count}/{len(cases)} "
        f"({fail_count} FAILED{skipped_str})",
        flush=True
    )
    return fail_count


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

    with TestHeaderFooter("AML Tests"):
        ret = run_tests(test_cases, test_runner, test_compiler, bin_dir)
    sys.exit(ret)


if __name__ == "__main__":
    main()
