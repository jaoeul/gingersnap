#! /usr/bin/python3
#
# Fuck make lol
#
# High level build process:
#
# - For each module
#   - If we are building the real application, ignore tests, and vice versa.
#   - Create ./build/<module>
#   - Get the soures for the module
#   - For each .c file in the sources, compile with given cflags. Do not link.
#     Output object path will be ./build/<module>/<file>.o
# - Get each object file in ./build
# - Link all object files together with given linker flags.

import os
import subprocess
import sys

def get_object_files(mode, modules):
    objects = []
    print(mode)
    for module in modules:

        # The test binary has its own main function. Never compile together with
        # the real application.
        if (mode == "debug" or mode == "release" or mode == "auto") and module == "tests":
            continue
        elif mode == "test" and module == "main":
            continue

        for path in os.listdir("./build/" + module):
            full_path = os.path.join(module, path)
            objects.append("./build/" + full_path)
    return objects

def build_output_path(src_file):
    output_path = "./build/" + src_file
    output_path = output_path[:-1] + "o"
    return output_path

def compile_src(mode, target, src_dir, cflags, linker_flags):
    modules = os.listdir(src_dir)
    for module in modules:
        if (mode == "debug" or mode == "release" or mode == "auto") and module == "tests":
            continue

        # Create build dir for module
        result = subprocess.run(["mkdir", f"./build/{module}", "-p"])
        print(result)
        if result.returncode != 0:
            exit(1)

        module_src = []
        for path in os.listdir(src_dir + "/" + module):
            full_path = os.path.join(module, path)
            module_src.append(full_path)

        # Compile modules, do not link.
        for src in module_src:
            if ".c" in src:
                output_path = build_output_path(src)
                compile_cmd = ["gcc"]
                compile_cmd = compile_cmd + cflags
                compile_cmd = compile_cmd + ["-c", src_dir + "/" + src, "-o", output_path]
                result = subprocess.run(compile_cmd)
                print(result)
                if result.returncode != 0:
                    exit(1)

    # Link module objects to create the target binary.
    objects  = get_object_files(mode, modules)
    link_cmd = ["gcc"]
    link_cmd = link_cmd + linker_flags
    link_cmd = link_cmd + objects
    link_cmd.append("-o")
    link_cmd.append(target)
    print(subprocess.run(link_cmd))

if __name__ == "__main__":
    debug_target   = "debug_gingersnap"
    auto_target    = "auto_gingersnap"
    release_target = "release_gingersnap"
    test_target    = "test_gingersnap"

    # Flags common for all builds.
    general_cflags       = ["-Werror", "-Wall"]
    general_linker_flags = ["-Werror", "-Wall", "-pthread"]

    # Build with debug instrumentation.
    debug_cflags = ["-g", "-DEMU_DEBUG", "-O0"];
    debug_cflags.extend(general_cflags)
    debug_linker_flags = ["-g"];
    debug_linker_flags.extend(general_linker_flags)

    # Test cflags
    test_cflags = ["-g", "-DEMU_TEST"]

    # Surpresses output. For automatic singlestepping through the emulator and gdb simultaneously,
    # using the autodebug_emu.py script.
    auto_cflags = ["-g", "-DAUTO_DEBUG"];
    auto_cflags.extend(general_cflags)
    auto_linker_flags = ["-g"];
    auto_linker_flags.extend(general_linker_flags)

    # Optimized build.
    release_cflags = ["-O3", "-DEMU_RELEASE", "-g"];
    release_cflags.extend(general_cflags)
    release_linker_flags = ["-O3"];
    release_linker_flags.extend(general_linker_flags)

    # Default target is debug.
    if len(sys.argv) < 2:
        compile_src("debug", debug_target, "./src", debug_cflags, debug_linker_flags)
    else:
        if sys.argv[1] == "d":
            compile_src("debug", debug_target, "./src", debug_cflags, debug_linker_flags)
        elif sys.argv[1] == "a":
            compile_src("auto", auto_target, "./src", auto_cflags, auto_linker_flags)
        elif sys.argv[1] == "r":
            compile_src("release", release_target, "./src", release_cflags, release_linker_flags)
        elif sys.argv[1] == "t":
            compile_src("test", test_target, "./src", test_cflags, debug_linker_flags)
        elif sys.argv[1] == "c":
                print(subprocess.run(["rm", "-rf", "./build"]))
                print(subprocess.run(["rm", debug_target]))
                print(subprocess.run(["rm", release_target]))
                print(subprocess.run(["rm", test_target]))
