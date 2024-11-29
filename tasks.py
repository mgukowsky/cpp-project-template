import json
import os
import platform
import shutil
import tempfile

from invoke import Context, task


def default_toolchain():
    system = platform.system()
    if system == "Windows":
        return "msvc_debug"
    elif system == "Linux":
        return "clang_debug"
    else:
        raise RuntimeError("Could not determine default toolchain for system")


def link_ccdb(preset: str):
    os.symlink(
        f"{os.getcwd()}/build/{preset}/compile_commands.json",
        "compile_commands.json.new",
    )
    os.replace("compile_commands.json.new", "compile_commands.json")


@task
def build(c: Context, preset: str = default_toolchain()):
    c.run(
        f"cmake --preset {preset} && cmake --build --preset {preset}",
        echo=True,
        pty=True,
    )
    link_ccdb(preset)


@task
def lint(c: Context, fix: bool = False):
    with open("compile_commands.json", "r") as f:
        ccjson = json.load(f)

    # strip out 3rd-party files we don't want to analyze
    ccs = [cc for cc in ccjson if "/_deps/" not in cc["file"]]

    outdir = tempfile.mkdtemp()
    with open(f"{outdir}/compile_commands.json", "w") as outf:
        json.dump(ccs, outf)

    c.run(
        f"run-clang-tidy '-header-filter=include/mgfw' -use-color -j{os.cpu_count()} -p {outdir} {"-fix -format" if fix else ""}",
        echo=True,
        pty=True,
    )

    shutil.rmtree(outdir, ignore_errors=True)


@task(default=True)
def workflow(c: Context, preset: str = default_toolchain()):
    c.run(f"cmake --workflow --preset {preset}", echo=True, pty=True)
    link_ccdb(preset)
