def _main():
    import os, sys, subprocess
    import mitsuba  # This import will check runtime requirements (ex: DrJit version)

    for p in sys.path:
        mi_package = os.path.join(p, "mitsuba")
        if os.path.isdir(mi_package):
            os_ext = ""
            env = os.environ.copy()
            if os.name == "nt":
                os_ext = ".exe"
                env["PATH"] = os.path.join(p, "drjit") + os.pathsep + env["PATH"]

            mi_executable = os.path.join(mi_package, "mitsuba" + os_ext)
            args = [mi_executable] + sys.argv[1:]
            process = subprocess.run(args, env=env)

            exit(process.returncode)

    print("Could not find mitsuba executable!", file=sys.stderr)
    exit(-1)


if __name__ == "__main__":
    _main()
