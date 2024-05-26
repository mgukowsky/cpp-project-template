from invoke import task, Context
import os

DEFAULT_TOOLCHAIN="clang_debug"

def link_ccdb(preset: str):
    os.symlink(f"{os.getcwd()}/build/{preset}/compile_commands.json", "compile_commands.json.new")
    os.replace("compile_commands.json.new", "compile_commands.json")

@task
def build(c: Context, preset: str = DEFAULT_TOOLCHAIN):
    c.run(f"cmake --preset {preset} && cmake --build --preset {preset}", echo=True, pty=True) 
    link_ccdb(preset)

@task(default=True)
def workflow(c: Context, preset: str = DEFAULT_TOOLCHAIN):
    # TODO: extrapolate default workflow based on platform
    c.run(f"cmake --workflow --preset {preset}", echo=True, pty=True) 
    link_ccdb(preset)
