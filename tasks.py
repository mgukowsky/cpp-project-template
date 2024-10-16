from invoke import task, Context
import os
import platform


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


@task(default=True)
def workflow(c: Context, preset: str = default_toolchain()):
    c.run(f"cmake --workflow --preset {preset}", echo=True, pty=True)
    link_ccdb(preset)
