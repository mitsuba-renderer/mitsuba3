# Running Mitsuba3 in Windows Subsystem for Linux (WSL2)

If you need to compile in a different OS than Linux then these instructions may be somewhat helpful but you'll have to work out what to do yourself in Windows, for example.

## Compiling source code

This in effect worked "out of the box" by following the [Mitsuba3 compilation from source instructions](https://mitsuba.readthedocs.io/en/stable/src/developer_guide/compiling.html), but to make it clear, the steps are as follows:

* Prerequisites: git, Python, pip, virtualenv
* Create a virtual environment for this: `virtualenv <env_name>` then `cd <env_name>` then `source bin/activate`
* Clone the fork of mitsuba3 recursively in order to capture the submodules: `git clone --recursive https://github.com/UoMResearchIT/mitsuba3-manchester`
* Install the relevant build tools using apt:
  * `sudo apt install clang-15 libc++-15-dev libc++abi-15-dev cmake ninja-build` (some of these may already be installed; you could also choose a different `clang` version should you wish to)
  * `sudo apt install libpng-dev libjpeg-dev` (for image I/O)
  * `sudo apt install libpython3-dev python3-distutils` (may be unnnecessary if you already have python installed)
* Export relevant environment variables: `export CC=clang-15`, `export CXX=clang++-15` (ideally, add these to your `~/.bashrc` file)
* Now build (from inside the mitsuba3-manchester directory that was created when cloning):
  * `mkdir build`
  * `cd build`
  * `cmake -GNinja ..`
  * (note: at this point it will tell you to edit the `mitsuba.conf` file to add any extra mitsuba variants that you may wish to use e.g. cuda/llvm; you can edit the conf file and then rerun the cmake command)
  * `ninja`
  * (note: it is probably possible to build using a system tool other than `ninja` but the parallel building helps a lot)
  * `source setpath.sh` (note: repeat this step every time you go into the virtual environment, or work out some way of adding it to the `activate` script)

This will give you the option of using `mitsuba` from the command line as well as making it accessible from Python scripts.

If you edit any C code, all that's required to recompile is to (re)run the `ninja` command from the `build` directory.

Finally, you may find that you'll need to install requirements for mitsuba3 as appropriate; just use `pip` to do this.

## Running examples

The repository contains some examples of code, though you will likely find that some of them may not work, especially any which convert the (large) original CSV file created by the particle physics codebase Geant4 with ~10^6 photons in it. It's possible to edit the scripts to just run examples with "converted" photon files, which can be found for example in the `Single_Emitter/csv` directory, but you will have to come up with some way of generating photons for your particular use case, since we always use photons created in Geant4. The format of these files is relatively obvious from looking at them, but do contact (see below) if you have any questions.

Most of the examples that have been run are the python scripts in the `Single_Emitter` directory, so please look at those to get ideas for your examples. It is likely that you will also have to build a scene for your simulations, to find out more about this look at the [mitsuba documentation](https://mitsuba.readthedocs.io/en/stable/index.html).

Note: the repository also contains Jupyter notebooks; it's possible to set up your environment to be able to then run notebooks in a Windows browser.  Follow the instructions at [https://code.adonline.id.au/jupyter-notebook-in-windows-subsystem-for-linux-wsl/](https://code.adonline.id.au/jupyter-notebook-in-windows-subsystem-for-linux-wsl/).

## Contact

If you have any issues please contact [andrew.gait@manchester.ac.uk](mailto:andrew.gait@manchester.ac.uk).

## Contributing

If after reading and understanding our implementation you wish to contribute to it then please feel free to make pull requests with any changes and they will be reviewed in the usual manner.