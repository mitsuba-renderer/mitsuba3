#!/usr/bin/env python
"""
Usage: tag_wheel_manylinux.py {dest_dir} {wheel_file}

This script manually replaces the '*-linux_x86_64' tag of a Python wheel
`wheel_file` with the following tags:
- '*-manylinux_2_17_x86_64'
- '*-manylinux2014_x86_64'

The new wheel is written to `dest_dir` with a filename that reflects the new
tags.
"""

import sys
import os
from zipfile import ZipFile
import re
import pathlib

def process_file(file, file_info, out_zip):
    """
    Copies the input file `file` to the archive `out_zip` without any
    modifications, unless the file is the metadata "WHEEL" file in which case
    the tags in it are modified.
    """
    if file_info.filename == f'{wheel_package_prefix}.dist-info/WHEEL':
        metadata = file.read().decode("utf-8")
        metadata_lines = metadata.split("\n")
        new_metadata_lines = []

        for i in range(len(metadata_lines)):
            line = metadata_lines[i]
            if re.match("^Tag: .*-linux_x86_64$", line):
                new_line1 = line.replace("-linux_x86_64", "-manylinux_2_17_x86_64")
                new_line2 = line.replace("-linux_x86_64", "-manylinux2014_x86_64")

                new_metadata_lines.append(new_line1)
                new_metadata_lines.append(new_line2)
            else:
                new_metadata_lines.append(line)

        out_zip.writestr(file_info.filename, '\n'.join(new_metadata_lines))
    else:
        out_zip.writestr(file_info.filename, file.read())

if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise RuntimeError("Exactly two arugments are expected: the output "
        "destination and the wheel archive file!")

    output_dir = sys.argv[1]
    wheel_loc = sys.argv[2]

    wheel_filename = os.path.basename(wheel_loc)
    wheel_package_prefix = wheel_filename[:wheel_filename.find('-', wheel_filename.find('-') + 1)]

    pathlib.Path(output_dir).mkdir(parents=True, exist_ok=True)
    output_wheel_filename = wheel_filename.replace(
            "-linux_x86_64", "-manylinux_2_17_x86_64.manylinux2014_x86_64")

    with ZipFile(f'{output_dir}/{output_wheel_filename}', 'w') as out_zip:
        with ZipFile(wheel_loc, mode="r") as wheel_zip:
            for file_info in wheel_zip.infolist():
                with wheel_zip.open(file_info) as file:
                    process_file(file, file_info, out_zip)
