# uACPI

A portable and easy-to-integrate implementation of the Advanced Configuration and Power Interface (ACPI).

[![CI](https://github.com/UltraOS/uACPI/actions/workflows/main.yml/badge.svg)](https://github.com/UltraOS/uACPI/actions/workflows/main.yml)

> [!WARNING]
> The project is still in active development and not yet ready for production use!  
> [Known issues](#state-of-the-project)

## State of the project

What works:
- The interpreter itself, all of AML is supported and relatively well-tested
- Namespace initialization, this includes properly running _STA and _INI, _REG for opregions
- The operation region subsystem. This includes public API for registering handlers, as well as builtin handlers for most common address space types
- Notify handlers including public API
- The resource subsystem. Every resource defined by ACPI 6.5 (last release) is supported
- Sleep API, allowing transition to any sleep state, wake vector programming API
- Fairly advanced event subsystem, supporting GPE/fixed events, wake, implicit notify, AML handlers
- GAS read/write API, FADT register read/write all implemented
- PCI routing table retrieval & interrupt model API
- Device search API

What's missing or doesn't work:
- ACPI global lock is not implemented
- No locking. This means all of the currently defined public API is not thread safe
- A lot of general utility API is missing and/or is currently internal-only
- Public API is not set in stone and may change, get added/removed without notice

## Why would I use this over ACPICA?

Whilst ACPICA is an old and battle-tested project, it still has some fundumental issues that make it very far from perfect or ideal.

### TLDR:
- Much better compatibility with the Windows NT object implicit-cast semantics than ACPICA
- AML reference semantics 100% compatbile with the Windows AML interpreter, **including edge cases**.
- A more sophisticated and safe object lifetime tracking without extra overhead (**AML that would crash the NT interpreter works just fine!**)
- unlike ACPICA, uACPI doesn't try to work around AML code designed for the Windows NT ACPI driver, instead, it embraces it
- No design flaws preventing true multi-threaded uses without the global interpreter lock

### More detailed overview
Expressions within package:
```asl
Method (TEST) {
    Local0 = 10
    Local1 = Package { Local0 * 5 }
    Return (DerefOf(Local1[0]))
}

// ACPICA: AE_SUPPORT, Expressions within package elements are not supported
// Windows, uACPI: Local0 = 50
Local0 = TEST()
```

Packages outside of a control method:
```asl
// ACPICA: internal error
// Windows, uACPI: ok
Local0 = Package { 1 }
```

Reference rebind semantics:
```asl
Local0 = 123
Local1 = RefOf(Local0)

// ACPICA: Local1 = 321, Local0 = 123
// Windows, uACPI: Local1 = reference->Local0, Local0 = 321
Local1 = 321
```

Increment/Decrement:
```asl
Local0 = 123
Local1 = RefOf(Local0)

// ACPICA: error
// Windows, uACPI: Local0 = 124
Local1++
```

ToHexString:
```asl

// ACPICA: "000000000000000F"
// Windows, uACPI: "0xF"
Local0 = ToHexString(0xF)
```

Multilevel references:
```asl
Local0 = 123
Local1 = RefOf(Local0)
Local2 = RefOf(Local1)

// ACPICA: Local3 = reference->Local0
// Windows, uACPI: Local3 = 123
Local3 = DerefOf(Local2)
```

Implict-cast semantics:
```asl
Name (TEST, "BAR")

// ACPICA: TEST = "00000000004F4F46"
// Windows, uACPI: TEST = "FOO"
TEST = 0x4F4F46
```

Returning a reference to a local object:
```asl
Method (TEST) {
    Local0 = 123

    // Use-after-free in ACPICA, perfectly fine in uACPI
    Return (RefOf(Local0))
}

Method (FOO) {
    Name (TEST, 123)

    // Use-after-free in ACPICA, object lifetime prolonged in uACPI (node is still removed from the namespace)
    Return (RefOf(TEST))
}
```

There's even more examples, but this should be enough to demonstrate the fundumental differences in designs.

## Integrating into a kernel

### 1. Add uACPI sources & include directories into your project

#### If you're using CMake
Simply add the following lines to your cmake:
```cmake
include(uacpi/uacpi.cmake)

target_sources(
    my-kernel
    PRIVATE
    ${UACPI_SOURCES}
)

target_include_directories(
    my-kernel
    PRIVATE
    ${UACPI_INCLUDES}
)
```

#### If you're using Meson
Add the following lines to your meson.build:
```meson
uacpi = subproject('uacpi')

uacpi_sources = uacpi.get_variable('sources')
my_kernel_sources += uacpi_sources

uacpi_includes = uacpi.get_variable('includes')
my_kernel_includes += uacpi_includes
```

#### Any other build system
- Add all .c files from [source](source) into your target sources
- Add [include](include) into your target include directories

### 2. Implement/override platform-specific headers

uACPI defines all platform-specific functionality in a few headers inside [include/uacpi/platform](include/uacpi/platform)

All of the headers can be "implemented" by your project in a few ways:
- Implement the expected helpers exposed by the headers
- Replace the expected helpers by your own and override uACPI to use them by defining the respective `UACPI_OVERRIDE_X` variable.
In this case, the header becomes a proxy that includes a corresponding `uacpi_x.h` header exported by your project.

Currently used platform-specific headers are:
- [arch_helpers.h](include/uacpi/platform/arch_helpers.h) - defines architecture/cpu-specific helpers
- [compiler.h](include/uacpi/platform/compiler.h) - defines compiler-specific helpers like attributes and intrinsics.
This already works for MSVC, clang & GCC so you most likely won't have to override it.
- [stdlib.h](include/uacpi/platform/stdlib.h) - exports a minimal subset of libc helpers that uACPI utilizes.
This should only be overriden if your kernel's standard library is fundumentally different from libc.
- [types.h](include/uacpi/platform/types.h) - typedefs a bunch of uacpi-specific types using the `stdint.h` header. You don't have to override this
unless you don't provide `stdint.h`.

### 3. Implement kernel API

uACPI relies on kernel-specific API to do things like mapping/unmapping memory, writing/reading to/from IO, PCI config space, and many more things.

This API is declared in [kernel_api.h](include/uacpi/kernel_api.h) and is implemented by your kernel.

### 4. Initialize uACPI

That's it, uACPI is now integrated into your project.

You should proceed to initialization.
There is currently no in-depth tutorial on how to do it, but you can refer to [test_runner.cpp](https://github.com/UltraOS/uACPI/blob/master/tests/runner/test_runner.cpp#L108-L133)
to see an example of how this can be done.

All of the headers and APIs defined in [uacpi](include/uacpi/) are public and may be utilized by your project.  
Anything inside [uacpi/internal](include/uacpi/internal) is considered private/undocumented and unstable API.

## Developing and contributing

Most of development work is fully doable in userland using the test runner.

### Setting up an IDE:

Simply open [tests/runner/CMakeLists.txt](tests/runner/CMakeLists.txt) in your favorite IDE.

For Visual Studio:
```
cd tests\runner && mkdir build && cd build && cmake ..
```

Then just simply open the .sln file generated by cmake.

### Running the test suite:
```
./tests/run_tests.py
```

If you want to contribute:
- Commits are expected to be atomic (changing one specific thing, or introducing one feature) with detailed description (if one is warranted for), an S-o-b line is welcome
- Code style is 4-space tabs, 80 cols, the rest can be seen by just looking at the current code

**All contributions are very welcome!**

## License

<a href="https://opensource.org/licenses/MIT">
  <img align="right" height="96" alt="MIT License" src="https://branding.cute.engineering/licenses/mit.svg" />
</a>

uACPI is licensed under the **MIT License**.  
The full license text is provided in the [LICENSE](LICENSE) file inside the root directory.
