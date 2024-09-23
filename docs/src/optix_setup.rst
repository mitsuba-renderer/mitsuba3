.. _optix-wsl2:

Mitsuba on WSL 2
----------------

Mitsuba uses the `NVIDIA OptiX
<https://developer.nvidia.com/rtx/ray-tracing/optix>`__ framework for
hardware-accelerated ray tracing. While OptiX is not yet officially supported
on the `Windows Subsystem for Linux 2 (WSL 2)
<https://learn.microsoft.com/en-us/windows/wsl/compare-versions#whats-new-in-wsl-2>`__,
it *is* possible to get it to run in practice. The following instructions are
based on an NVIDIA `forum post by @dhart
<https://forums.developer.nvidia.com/t/problem-running-optix-7-6-in-wsl/239355/8>`__
Use them at your own risk.

- Determine your driver version using the ``nvidia-smi`` command. This is a number
  such as ``560.94`` shown in the middle of the first row.

- Go to the `NVIDA driver webpage <https://www.nvidia.com/en-us/drivers>`__ to
  download a similar driver version for Linux (64 bit). You may have to click on
  the "New feature branch" tab to find newer driver versions.

- Following this step, you should have file named
  ``NVIDIA-Linux-x86_64-*.run``. Move it to your WSL home directory but *do
  not install it*. Instead, merely extract its contents within a WSL session using
  the following command:

.. code-block:: console

   $ bash NVIDIA-Linux-x86_64-*.run -x --target driver

Create a symbolic link that exposes the already installed CUDA driver to runtime loading:

.. code-block:: console

   $ ln -s /usr/lib/wsl/lib/libcuda.so /usr/lib/x86_64-linux-gnu/

Next, copy-paste and run the following command:

.. code-block:: console

   $ mkdir driver-dist && cp driver/libnvoptix.so.* driver-dist/libnvoptix.so.1 && cp driver/libnvidia-ptxjitcompiler.so.* driver-dist/libnvidia-ptxjitcompiler.so.1 && cp driver/libnvidia-rtcore.so.* driver-dist && cp driver/libnvidia-gpucomp.so.* driver-dist && cp driver/nvoptix.bin driver-dist && explorer.exe driver-dist && explorer.exe "C:\Windows\System32\lxss\lib"

This will open two Explorer windows: one to a system path containing internal
WSL driver files (``C:\Windows\System32\lxss\lib``), and another to a newly
created ``driver-dist`` directory containing files that need to be copied to
``C:\Windows\System32\lxss\lib``. Perform this copy manually using Explorer and
overwrite existing files if present. It will warn you that this is dangerous,
and you will need to give permission.

Close all WSL windows, and enter the following command in a ``cmd.exe`` or
PowerShell session:

.. code-block:: pwsh-session

   C:\Users\...> wsl --shutdown

Following this, OptiX should be usable within WSL.

.. warning::

   Using CUDA and OptiX through WSL degrades performance. Please do not collect
   performance data within WSL, since the results will not be representative.
