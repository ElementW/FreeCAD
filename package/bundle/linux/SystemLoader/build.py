#!/usr/bin/env python3
# pylint: disable=missing-class-docstring,missing-function-docstring
"""
Build forwarder libraries for use within an AppImage.
Takes in existing .so files and replaces them with libraries forwarding functions to the
host system one loaded in a glibc dynamic loader namespace (see `dlmopen(3)`).
Allows for loading system libraries we cannot bundle, such as OpenGL, without them clashing
with the possibly outdated libs that are already loaded by that point.
"""

import argparse
import dataclasses
import os
import subprocess
from datetime import datetime
from io import StringIO
from multiprocessing.pool import ThreadPool
from pathlib import Path
from typing import Set, List
from elftools.elf.elffile import ELFFile
from elftools.elf.dynamic import DynamicSegment

loader_source_dir = Path(__file__).resolve().parent


def compile_c(
    source: str, output_path: Path, *, kind: str, flags: List[str | Path] | None = None
):
    start = datetime.now()
    result = subprocess.run(
        [
            os.environ.get("CC", "gcc"),
            *(flags or []),
            "-Wl,--build-id=none",
            "-Wl,--no-undefined",
            "-shared",
            "-fPIC",
            "-fno-gnu-tm",
            "-nostartfiles",
            "-falign-functions=8",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-fvisibility=hidden",
            "-fomit-frame-pointer",
            "-O2",
            "-x",
            "c",
            "-o",
            output_path.resolve(),
            "-",
        ],
        input=f'#line 1 "{output_path.name}.c"\n{source}'.encode(),
        cwd=loader_source_dir,
    )
    if result.returncode != 0:
        print(source)
        result.check_returncode()  # Raise exception
    print(
        f"Compiled {kind} {output_path} in {(datetime.now() - start).total_seconds():.3f} s"
    )


@dataclasses.dataclass
class SoInfo:
    soname: str
    exported_symbols: Set[str]


def extract_so_info(path: Path) -> SoInfo:
    with path.open("rb") as io:
        elf = ELFFile(io)
        try:
            dynamic_segment = next(
                s for s in elf.iter_segments() if isinstance(s, DynamicSegment)
            )
            soname_tag = next(
                t for t in dynamic_segment.iter_tags() if t["d_tag"] == "DT_SONAME"
            )
            soname = soname_tag.soname
        except StopIteration:
            # pylint: disable-next=raise-missing-from
            raise ValueError(f"{path} has no dynamic segment or SONAME")
        dynsym = elf.get_section_by_name(".dynsym")
        return SoInfo(
            soname=soname,
            exported_symbols=set(
                sym.name
                for sym in dynsym.iter_symbols()
                if sym.entry.st_value != 0
                and sym.entry.st_shndx != "SHN_UNDEF"
                and sym.entry.st_info.bind in ("STB_GLOBAL", "STB_WEAK")
                and sym.entry.st_info.type == "STT_FUNC"
                and sym.name not in ("", "_init", "_fini")
            ),
        )


library_template = (loader_source_dir / "lib_template.c").read_text()


def remove_zero_symbols(library_path: Path, *, verbose: bool = False):
    library_path = library_path.absolute()
    print(f"Removing {library_path}: zero symbols exported")
    # Remove the version symlinks as well
    link = library_path
    while True:
        # Can't use link.resolve() because the target file will already be gone
        if link.exists() and (link == library_path or link.readlink() == library_path):
            if verbose:
                print(f"Removing link {library_path}")
            link.unlink()
        next_link = link.with_suffix("")
        if next_link.name == link.name:
            break
        link = next_link


def build_library(
    library_path: Path,
    loader_library_path: Path,
    kind: str,
    *,
    dump_source: bool = False,
    verbose: bool = False,
):
    """
    Create the source code for a library's equivalent according to ``kind``, and compile it.
    """
    so_info = extract_so_info(library_path)
    if len(so_info.exported_symbols) == 0:
        remove_zero_symbols(library_path, verbose=verbose)
        return
    source = StringIO()
    source.write(f'#define SONAME "{library_path.name}"\n')
    source.write(f"#define KIND_{kind}\n")
    source.write(f"#define SYMBOL_COUNT {len(so_info.exported_symbols)}\n")
    source.write("#define SYMBOL_LIST(X) \\\n")
    for i, symbol_name in enumerate(so_info.exported_symbols):
        source.write(f"    X({symbol_name}, {i}) \\\n")
    source.write("\n\n")
    source.write(library_template)
    if dump_source:
        with library_path.with_name(f"{library_path.name}.c").open("w") as src:
            src.write(source.getvalue())
    library_path.unlink()
    compile_c(
        source.getvalue(),
        library_path,
        kind=kind.lower(),
        flags=[
            "-L",
            loader_library_path.parent,
            f"-l{loader_library_path.stem.replace('lib', '')}",
            f"-Wl,-soname,{so_info.soname}",
            "-Wl,-rpath,$ORIGIN/",
        ],
    )
    assert library_path.is_file()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
    )
    parser.add_argument(
        "--loader-output",
        type=Path,
        required=True,
    )
    parser.add_argument("--dump-source", action="store_true")
    parser.add_argument(
        "--crash",
        type=Path,
        nargs="+",
    )
    parser.add_argument(
        "--forward",
        type=Path,
        nargs="+",
    )
    return parser.parse_args()


def process_cli_arg(
    path: Path, loader_library_path: Path, kind: str, args: argparse.Namespace
):
    so_file_path = path.absolute()
    if so_file_path.is_symlink():
        if args.verbose:
            print(f"Skipping {so_file_path}: symlink to {so_file_path.readlink()}")
        return
    # Don't run this before the symlink check: .exists() follows symlinks.
    if not so_file_path.exists():
        raise FileNotFoundError(so_file_path)
    build_library(
        so_file_path,
        loader_library_path=loader_library_path,
        kind=kind,
        dump_source=args.dump_source,
        verbose=args.verbose or False,
    )


def main():
    args = parse_args()
    loader_library_path = args.loader_output.resolve()
    if not loader_library_path.parent.is_dir():
        raise NotADirectoryError(
            f"Output lib dir {loader_library_path.parent} doesn't exist"
        )
    if (
        not loader_library_path.name.startswith("lib")
        or not loader_library_path.suffix != "so"
    ):
        raise ValueError("Loader library name must be of the libXYZ.so format")
    compile_c(
        (loader_source_dir / "loader.c").read_text(), loader_library_path, kind="loader"
    )
    with ThreadPool(os.cpu_count()) as pool:
        pool.map(
            lambda p: process_cli_arg(p, loader_library_path, "FORWARDER", args),
            args.forward or [],
        )
        pool.map(
            lambda p: process_cli_arg(p, loader_library_path, "CRASHER", args),
            args.crash or [],
        )


if __name__ == "__main__":
    main()
