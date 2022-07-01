#!/usr/bin/env python
"""
Usage: tag_wheel_manylinux.py {wheel_file} {dest_dir}

This script manually replaces the '*-linux_x86_64' tag of a Python wheel
`wheel_file` with the following tags:
- '*-manylinux_2_17_x86_64'
- '*-manylinux2014_x86_64'

The new wheel is written to `dest_dir` with a filename that reflects the new
tags.
"""

import os
import pathlib
import re
import shutil
import subprocess
import sys


def process_wheel_info_file(file):
    """
    Modifies the metadata "WHEEL" file given as `file` such to have the
    appropriate 'manylinux' platform tags instead of the generic 'linux' one.
    """
    metadata_lines = file.read().splitlines()
    new_metadata_lines = []

    for i in range(len(metadata_lines)):
        line = metadata_lines[i]
        if re.match("^Tag: .*-linux_x86_64$", line):
            new_line = line.replace("-linux_x86_64", "-manylinux_2_17_x86_64")
            new_metadata_lines.append(new_line)

            new_line = line.replace("-linux_x86_64", "-manylinux2014_x86_64")
            new_metadata_lines.append(new_line)
        else:
            new_metadata_lines.append(line)

    file.seek(0)
    file.write("\n".join(new_metadata_lines))
    file.truncate()


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise RuntimeError("Exactly two arugments are expected: the wheel "
                           "archive file and the output directory!")

    wheel_loc = sys.argv[1]
    output_dir = sys.argv[2]

    wheel_filename = os.path.basename(wheel_loc)
    version_begin = wheel_filename.find("-")
    wheel_package_prefix = wheel_filename[: wheel_filename.find("-", version_begin + 1)]

    unpack_dir = f"/tmp/{os.path.splitext(os.path.basename(__file__))[0]}/{wheel_package_prefix}"
    shutil.rmtree(unpack_dir, ignore_errors=True)
    os.makedirs(unpack_dir, exist_ok=False)
    os.makedirs(output_dir, exist_ok=True)

    # The `wheel` package drops all file permissions when unpacking
    subprocess.check_call(["unzip", "-q", wheel_loc, "-d", unpack_dir])

    with open(
        f"{unpack_dir}/{wheel_package_prefix}.dist-info/WHEEL",
        "r+",
    ) as wheel_info_file:
        process_wheel_info_file(wheel_info_file)

    # The `wheel` package will recompute the hash of every file and modify the RECORD file
    # It also renames the packed wheel to reflect the new platform tags
    subprocess.check_call([
        sys.executable, "-m",
        "wheel", "pack", f"{unpack_dir}", "-d", output_dir,
    ])
