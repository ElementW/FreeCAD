#!/usr/bin/env python3
import argparse
import json
import os
import platform
import re
import subprocess
from abc import ABC, abstractmethod
from collections import defaultdict
from dataclasses import dataclass, field
from datetime import datetime
from io import StringIO
from pathlib import Path
from typing import Dict, Set, List, Union, Any, Iterable, Optional


@dataclass(frozen=True)
class Package:
    Name = str
    meta_path: Path
    name: Name
    depends: List[str]
    files: List[str]
    size_in_bytes: int
    executable_paths: List[Path]


def executables_in_file_list(root: Path, files: Iterable[str]) -> List[Path]:
    executable_paths: List[Path] = []
    for file_path_str in files:
        # Use heuristics to speed up discrimination of executable and non-executable files
        definitely_executable = (
            file_path_str.endswith((".so", ".dylib", ".exe", ".dll"))
            or ".so." in file_path_str
        )
        maybe_executable = not definitely_executable and "." not in file_path_str
        # Skip files whose names don't indicate an executable
        if not (definitely_executable or maybe_executable):
            continue
        # Skip files that are gone
        executable_path = root / file_path_str
        if not executable_path.is_file():
            continue
        if definitely_executable:
            executable_paths.append(executable_path)
        if maybe_executable:
            try:
                Executable(executable_path)
                executable_paths.append(executable_path)
            except NotAnExecutableError:
                pass
    return executable_paths


def is_executable_a_library(path: Path) -> bool:
    return path.suffix in ("so", "dylib", "exe", "dll") or ".so." in path.name


class Environment:
    def __init__(self, root: Path):
        self.root = root
        conda_meta_path: Path = root / "conda-meta"
        if not conda_meta_path.is_dir():
            raise FileNotFoundError("conda-meta not in environment")

        self.packages: Dict[Package.Name, Package] = {}
        for meta_path in sorted(conda_meta_path.glob("*.json")):
            with meta_path.open("r") as meta_file:
                meta = json.load(meta_file)
            package = Package(
                meta_path=meta_path,
                name=meta["name"],
                depends=[d.split(" ", maxsplit=1)[0] for d in meta["depends"]],
                files=meta["files"],
                size_in_bytes=sum(
                    f["size_in_bytes"]
                    for f in meta["paths_data"]["paths"]
                    if f["path_type"] == "hardlink"
                ),
                executable_paths=executables_in_file_list(self.root, meta["files"]),
            )
            self.packages[package.name] = package

        self.rdepends: Dict[Package.Name, Set[Package.Name]] = defaultdict(set)
        for package in self.packages.values():
            for dependency_name in package.depends:
                self.rdepends.setdefault(dependency_name, set()).add(package.name)

        self.broken: Set[Package.Name] = set()
        self.removed: Dict[Package.Name, Package] = {}

        self.leaf_packages = set()
        leaves = self.orphans
        assert list(leaves) == ["freecad"]
        self.leaf_packages = leaves

        self.library_paths: Dict[str, Path] = {}
        for package in self.packages.values():
            for file in package.files:
                if ".so" in file or ".dylib" in file or ".dll" in file:
                    path = self.root / file
                    self.library_paths[path.name] = path
        self._library_exported_symbols_cache: Dict[str, Set[str]] = {}
        self.referenced_removed_symbols: Set[str] = set()

    @property
    def orphans(self) -> Set[Package.Name]:
        return set(self.packages.keys()) - self.rdepends.keys() - self.leaf_packages

    def remove_package(self, package_name: Package.Name):
        package = self.packages[package_name]
        # Mark any package that depends on the one being removed as broken.
        self.broken.update(self.rdepends[package_name])
        # Update reverse dependency map to remove the package.
        for dependency in package.depends:
            try:
                self.rdepends[dependency].remove(package_name)
                if len(self.rdepends[dependency]) == 0:
                    del self.rdepends[dependency]
            except KeyError:
                pass
        # Unmark broken on package being deleted, if applicable.
        try:
            self.broken.remove(package_name)
        except KeyError:
            pass
        self.removed[package_name] = package
        del self.packages[package_name]

    def patch_package(
        self,
        package_name: Package.Name,
        *,
        dry_run: bool,
        substitution_library_name: str,
        keep_needed_libs_pattern: Optional[re.Pattern[str]],
    ):
        package = self.packages[package_name]
        for executable_path in package.executable_paths:
            if executable_path.is_symlink():
                continue
            patch_result = Executable(executable_path).patch(
                self,
                dry_run=dry_run,
                substitution_library_name=substitution_library_name,
                keep_needed_libs_pattern=keep_needed_libs_pattern,
            )
            self.referenced_removed_symbols |= patch_result.referenced_removed_symbols

    def get_library_exported_symbols(self, library_name: str):
        try:
            return self._library_exported_symbols_cache[library_name]
        except KeyError:
            exe = Executable(self.library_paths[library_name])
            symbols = exe.exported_symbols
            self._library_exported_symbols_cache[library_name] = symbols
            return symbols


class NotAnExecutableError(RuntimeError):
    pass


@dataclass(frozen=True)
class PatchResult:
    referenced_removed_symbols: Set[str] = field(default_factory=set)


class Executable(ABC):
    def __new__(cls, path: Path):
        if cls is not Executable:
            return super(Executable, cls).__new__(cls)
        with path.open("rb") as io:
            magic = io.read(4)
            if magic == b"\x7fELF":
                return ELFExecutable.__new__(ELFExecutable, path)
            elif magic == b"\xfe\xed\xfa\xcf":
                return MachOExecutable.__new__(ELFExecutable, path)
            elif magic[:2] == b"MZ":
                return PEExecutable.__new__(ELFExecutable, path)
            raise NotAnExecutableError

    def __init__(self, path: Path):
        self.path = path

    @property
    @abstractmethod
    def exported_symbols(self) -> Set[str]: ...

    @property
    @abstractmethod
    def imported_symbols(self) -> Set[str]: ...

    @property
    @abstractmethod
    def needed_library_names(self) -> Set[str]: ...

    @abstractmethod
    def change_needed_libraries(self, *, add: Iterable[str], remove: Iterable[str]): ...

    def patch(
        self,
        env: Environment,
        *,
        dry_run: bool,
        substitution_library_name: str,
        keep_needed_libs_pattern: Optional[re.Pattern[str]],
    ) -> PatchResult:
        removed_library_names = set(
            Path(f).name
            for removed in env.removed.values()
            for f in removed.executable_paths
            if is_executable_a_library(f)
        )
        needed_library_names = self.needed_library_names
        libraries_to_patch_out = needed_library_names.intersection(
            removed_library_names
        )
        libraries_to_keep: Set[str] = set()
        if keep_needed_libs_pattern:
            for lib in libraries_to_patch_out:
                if keep_needed_libs_pattern.match(lib):
                    libraries_to_keep.add(lib)
            libraries_to_patch_out -= libraries_to_keep
        if len(libraries_to_patch_out) == 0:
            return PatchResult()
        print(
            f"  - {self.path.name}: {' '.join(sorted(libraries_to_patch_out))}"
            + (
                f' (keeping {" ".join(sorted(libraries_to_keep))} as requested)'
                if len(libraries_to_keep) > 0
                else ""
            )
        )

        if not dry_run:
            self.change_needed_libraries(
                add=(substitution_library_name,), remove=libraries_to_patch_out
            )

        exported_from_removed_libs = set(
            s
            for library_name in libraries_to_patch_out
            for s in env.get_library_exported_symbols(library_name)
        )
        imported = self.imported_symbols
        return PatchResult(
            referenced_removed_symbols=exported_from_removed_libs.intersection(imported)
        )


class ELFExecutable(Executable):
    @property
    def exported_symbols(self) -> Set[str]:
        with self.path.open("rb") as io:
            from elftools.elf.elffile import ELFFile

            elf = ELFFile(io)
            dynsym = elf.get_section_by_name(".dynsym")
            return set(
                sym.name
                for sym in dynsym.iter_symbols()
                if sym.entry.st_value != 0
                and sym.entry.st_shndx != "SHN_UNDEF"
                and sym.entry.st_info.bind == "STB_GLOBAL"
                and sym.name != ""
            )

    @property
    def imported_symbols(self) -> Set[str]:
        with self.path.open("rb") as io:
            from elftools.elf.elffile import ELFFile

            elf = ELFFile(io)
            dynsym = elf.get_section_by_name(".dynsym")
            return set(
                sym.name
                for sym in dynsym.iter_symbols()
                if sym.entry.st_value == 0
                and sym.entry.st_shndx == "SHN_UNDEF"
                and sym.entry.st_info.bind == "STB_GLOBAL"
                and sym.name != ""
            )

    @property
    def needed_library_names(self) -> Set[str]:
        needed_sonames: Set[str] = set()
        with self.path.open("rb") as io:
            from elftools.elf.elffile import ELFFile

            elf = ELFFile(io)
            try:
                dynamic = next(elf.iter_segments(type="PT_DYNAMIC"))
            except StopIteration:
                # ELF has no dynamic section
                return needed_sonames
            for tag in dynamic.iter_tags():
                if tag.entry.d_tag == "DT_NEEDED":
                    needed_sonames.add(tag.needed)
        return needed_sonames

    def change_needed_libraries(self, *, add: Iterable[str], remove: Iterable[str]):
        cmdline: List[Union[str, Path]] = ["patchelf"]
        for soname in add:
            cmdline.extend(("--add-needed", soname))
        for soname in remove:
            cmdline.extend(("--remove-needed", soname))
        cmdline.append(self.path)
        subprocess.run(cmdline, check=True)


class MachOExecutable(Executable, ABC):
    # TODO
    pass


class PEExecutable(Executable, ABC):
    # TODO
    pass


SUBSTITUTION_LIBRARY_SOURCE_PREAMBLE = r"""
#include <cstdio>

#if defined(WIN64) || defined(_WIN64) || defined(__WIN64__) || defined(__CYGWIN__)
#  include <atomic>
#  define EXPORT __declspec(dllexport)
#  define NOINLINE __declspec(noinline)
#  define fence() std::atomic_signal_fence(std::memory_order_seq_cst)
#  define unreachable __assume(0)
#else
#  define EXPORT __attribute__((weak))
#  define NOINLINE __attribute__((noinline))
#  define fence() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#  define unreachable __builtin_unreachable()
#endif

static int* volatile FreeCADSymbolSubst_deliberate_null = nullptr;
static const char constzeroes[1024] = {0};

NOINLINE [[noreturn]] static void FreeCADSymbolSubst_crash(const char* symbol) {
  fprintf(stderr,
    "%s() called yet it is removed from FreeCAD distribution.\n"
    "This is a packaging problem, please report the issue and include this error message at "
    "https://github.com/FreeCAD/FreeCAD/issues\n",
    symbol
  );
  fflush(stderr);
  fence();
  volatile int* p = FreeCADSymbolSubst_deliberate_null;
  *p = 42;
  fence();
  unreachable;
}

NOINLINE static void FreeCADSymbolSubst_log(const char* symbol) {
  fprintf(stderr, "%s() called but stubbed in FreeCAD distribution.\n", symbol);
}
"""


# Compiles a library file (.so/.dylib/.dll) to substitute for the missing symbols
# introduced as a result of removing some packages.
# Intentionally keeps duplicate crash functions as separate funcs instead
# of using symbol aliasing in order to preserve visible stack traces.
def build_substitution_library(
    substitution_rules: Dict[str, Any],
    referenced_removed_symbols: Set[str],
    output_path: Path,
):
    for r in [r for r in substitution_rules.keys() if r[0] == "$"]:
        del substitution_rules[r]
    rules_names = [*substitution_rules.keys()]
    rule_re = re.compile(f"^{'|'.join(f'({r})' for r in substitution_rules.keys())}$")
    source = StringIO()
    source.write(SUBSTITUTION_LIBRARY_SOURCE_PREAMBLE)

    def write_func(
        return_type: str,
        name: str,
        *,
        body: str = "",
        return_value: str = "",
        attr: str = "",
    ):
        source.write(f'extern "C" EXPORT {attr} {return_type} {name}() {{\n')
        if body:
            source.write(f"  {body}\n")
        if return_value != "":
            source.write(f"  return {return_value};\n")
        source.write("}\n\n")

    for symbol in referenced_removed_symbols:
        match = rule_re.match(symbol)
        if not match or match.lastindex <= 0:
            raise ValueError(f'No substitution rule for symbol "{symbol}"')
        rule = substitution_rules[rules_names[match.lastindex - 1]]
        forward: bool = rule.get("forward", False)
        if forward:
            source.write(f"static \n")
        if rule.get("crash", False) is True:
            write_func(
                "void",
                symbol,
                attr="[[noreturn]]",
                body=f'FreeCADSymbolSubst_crash("{symbol}");',
            )
        elif "value" in rule["return"]:
            write_func(
                rule["return"]["type"],
                symbol,
                body=f'FreeCADSymbolSubst_log("{symbol}");',
                return_value=rule["return"]["value"],
            )
        else:
            write_func(rule["return"]["type"], symbol)
    cxx = os.environ.get("CXX", "clang++" if platform.system() == "Darwin" else "g++")
    start = datetime.now()
    if cxx.lower() in ("cl", "cl.exe", "clang-cl", "clang-cl.exe"):
        raise NotImplementedError(
            "Compiling the substitution library with MSVC not yet implemented"
        )
    else:
        subprocess.run(
            [
                cxx,
                "-nostdlib",
                "-fno-optimize-sibling-calls",
                "-fno-omit-frame-pointer",
                "-fno-exceptions",
                "-Wl,--build-id=none",
                "-shared",
                "-fPIC",
                "-O0",
                "-x",
                "c++",
                "-o",
                output_path,
                "-",
            ],
            check=True,
            input=source.getvalue().encode(),
        )
    print(
        f"Compiled {output_path.name} in {(datetime.now() - start).total_seconds():.3f} s"
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-n", "--dry-run", action="store_true")
    parser.add_argument(
        "-e",
        "--environment",
        dest="environment_path",
        type=Path,
        required=True,
        help="Environment directory to work in (containing conda-meta).",
    )
    parser.add_argument(
        "--keep-needed-libs",
        dest="keep_needed_libs",
        type=re.compile,
        nargs="?",
        help="Regex of needed libraries (DT_NEEDED, ...) to keep.",
    )
    parser.add_argument(
        "-s",
        "--substitution-rules",
        dest="substitution_rules_path",
        type=Path,
        required=True,
        help="YAML file containing rules for building the symbol substitution library",
    )
    parser.add_argument(
        "--print-rdeps-before-removal", action="store_true"
    )
    parser.add_argument("package_globs", type=str, nargs="+")
    args = parser.parse_args()

    system = platform.system()
    if system in ("Linux", "FreeBSD", "OpenBSD"):
        substitution_library_path = (
            args.environment_path / "lib" / "libFreeCADSymbolSubst.so"
        )
    elif system == "Darwin":
        substitution_library_path = (
            args.environment_path / "lib" / "libFreeCADSymbolSubst.dylib"
        )
    elif system == "Windows":
        substitution_library_path = (
            args.environment_path / "bin" / "FreeCADSymbolSubst.dll"
        )
    else:
        raise NotImplementedError(f'OS "{system}" not supported')

    env = Environment(args.environment_path)
    if args.print_rdeps_before_removal:
        name_max_len = max(len(name) for name in env.rdepends.keys())
        for package_name, rdeps in sorted(env.rdepends.items()):
            print(f"{package_name:>{name_max_len}} ← {' '.join(rdeps)}")
    bytes_total = sum(r.size_in_bytes for r in env.packages.values())
    # glob.translate is not available prior to Python 3.13, implement a super basic version of "globs"
    package_name_re = re.compile(
        "|".join(re.escape(str(pg)).replace("\\*", ".*") for pg in args.package_globs)
    )
    packages_to_remove = [p for p in env.packages if package_name_re.match(p)]
    for package_name in packages_to_remove:
        env.remove_package(package_name)
    all_orphans: Set[Package.Name] = set()
    while len(orphans := env.orphans) > 0:
        all_orphans |= orphans
        for orphan in orphans:
            env.remove_package(orphan)
    bytes_to_remove = sum(r.size_in_bytes for r in env.removed.values())
    print(
        f"Will remove ({bytes_to_remove/1024/1024:.1f}/{bytes_total/1024/1024:.1f} MiB, "
        f"{bytes_to_remove/bytes_total*100:.1f}%):"
    )
    for removed in sorted(
        env.removed.values(), key=lambda r: r.size_in_bytes, reverse=True
    ):
        print(
            f"- {removed.name} ({removed.size_in_bytes/1024/1024:.2f} MiB"
            f"{', orphaned' if removed.name in all_orphans else ''})"
        )
    print("Patching:")
    for broken in env.broken:
        print(f"- {broken} (wants {' '.join(d for d in env.packages[broken].depends if d in env.removed)})")
        env.patch_package(
            broken,
            dry_run=args.dry_run,
            substitution_library_name=substitution_library_path.name,
            keep_needed_libs_pattern=args.keep_needed_libs,
        )
    print("Removing files")
    if not args.dry_run:
        for package in env.removed.values():
            for file_path_str in package.files:
                file_path = env.root / file_path_str
                file_path.unlink(missing_ok=True)
            package.meta_path.unlink()

    print("Referenced yet removed symbols:")
    for removed in sorted(env.referenced_removed_symbols):
        print(f"- {removed}")

    import yaml

    with args.substitution_rules_path.open("r") as io:
        substitution_rules = yaml.safe_load(io)
    build_substitution_library(
        substitution_rules, env.referenced_removed_symbols, substitution_library_path
    )


if __name__ == "__main__":
    main()
