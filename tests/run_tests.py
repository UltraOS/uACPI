#!/usr/bin/python3
import subprocess
import argparse
import os
import shutil
import sys
import platform
from typing import List, Tuple


def abs_path_to_current_dir() -> str:
    return os.path.dirname(os.path.abspath(__file__))


def get_case_name_and_expected_result(case: str) -> Tuple[str, str, str]:
    with open(case) as tc:
        name = tc.readline()
        name = name[name.find(":") + 1:].strip()

        expected_line = tc.readline()
        expected_line = expected_line[expected_line.find(":") + 1:].strip()
        expected = [val.strip() for val in expected_line.split("=>")]

        return name, expected[0], expected[1]


def compile_case(compiler: str, case: str, bin_dir: str) -> str:
    case_aml_name = os.path.basename(case).rsplit(".", 1)[0] + ".aml"
    out_case = os.path.join(bin_dir, case_aml_name)

    proc = subprocess.Popen([compiler, "-p", out_case, case],
                            stdout=subprocess.PIPE,
                            universal_newlines=True)

    proc.wait(10)
    stdout = proc.stdout
    assert stdout

    if proc.returncode != 0:
        raise RuntimeError(f"Compiler error: {stdout.read()}")

    return out_case


def run_tests(
    cases: List[str], runner: str, compiler: str,
    bin_dir: str
) -> int:
    fail_count = 0

    for case in cases:
        name, rtype, value = get_case_name_and_expected_result(case)
        compiled_case = compile_case(compiler, case, bin_dir)

        print(f"Running test '{name}'...", end=" ")
        proc = subprocess.Popen([runner, compiled_case, rtype, value],
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,
                                universal_newlines=True)
        proc.wait(10)

        if proc.returncode == 0:
            print("OK")
            continue

        print("FAIL")
        fail_count += 1

        print("TEST OUTPUT:")
        output = ""
        if proc.stdout:
            output += "from STDOUT:\n"
            output += proc.stdout.read()
        if proc.stderr:
            output += "from STDERR:\n"
            output += proc.stderr.read()

    print(
        f"SUMMARY: {len(cases) - fail_count}/{len(cases)} "
        f"({fail_count} FAILED)"
    )
    return fail_count


def test_relpath(*args: str) -> str:
    return os.path.join(abs_path_to_current_dir(), *args)


def test_runner_binary() -> str:
    out = "test-runner"

    if platform.system() == "Windows":
        out += ".exe"

    return out


def build_test_runner() -> str:
    build_dir = f"build-{platform.system().lower()}"
    runner_build_dir = test_relpath("runner", build_dir)
    runner_exe = os.path.join(runner_build_dir, test_runner_binary())

    if not os.path.isdir(runner_build_dir):
        os.makedirs(runner_build_dir, exist_ok=True)
        subprocess.run(["cmake", ".."], cwd=runner_build_dir, check=True)

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
    args = parser.parse_args()

    test_dir = args.test_dir
    test_runner = args.test_runner
    if test_runner is None:
        test_runner = build_test_runner()

    bin_dir = args.binary_directory
    os.makedirs(bin_dir, exist_ok=True)

    test_cases = [os.path.join(test_dir, f) for f in os.listdir(test_dir)]

    try:
        ret = run_tests(test_cases, test_runner, args.asl_compiler, bin_dir)
    finally:
        shutil.rmtree(bin_dir)

    sys.exit(ret)


if __name__ == "__main__":
    main()
