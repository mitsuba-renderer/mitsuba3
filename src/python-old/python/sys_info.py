'''
This script prints information about the system configuration regarding Mitsuba
and Dr.Jit. For instance, this information can be very helpful to other
developers when investigating an issue.

This code is highly inspired from `torch/utils.collect_env.py`
'''

if __name__ == '__main__':
    import os, sys, re
    import locale, platform, subprocess
    from importlib.machinery import SourceFileLoader
    from os.path import join, dirname, realpath, exists

    dir = dirname(realpath(__file__))

    import drjit as dr
    import mitsuba as mi
    mi.set_variant('scalar_rgb')

    # --------------------------------------------------------------------------
    #     Helper functions
    # --------------------------------------------------------------------------

    def run(command):
        'Returns (return-code, stdout, stderr)'
        p = subprocess.Popen(command,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE,
                             shell=True)
        raw_output, raw_err = p.communicate()
        rc = p.returncode
        if sys.platform.startswith('win32'):
            enc = 'oem'
        else:
            enc = locale.getpreferredencoding()
        output = raw_output.decode(enc)
        err = raw_err.decode(enc)
        return rc, output.strip(), err.strip()

    def run_and_match(command, regex):
        'Runs command and returns the first regex match if it exists'
        rc, out, _ = run(command)
        if rc != 0:
            return None
        match = re.search(regex, out)
        if match is None:
            return None
        return match.group(1)

    def get_gpu_info():
        smi = get_nvidia_smi()
        uuid_regex = re.compile(r' \(UUID: .+?\)')
        gpu_id_regex = re.compile(r'GPU .: ')
        rc, out, _ = run(smi + ' -L')
        if rc != 0:
            return None
        return re.sub(gpu_id_regex, '', re.sub(uuid_regex, '', out))

    def get_nvidia_smi():
        smi = 'nvidia-smi'
        if sys.platform.startswith('win32'):
            system_root = os.environ.get('SYSTEMROOT', 'C:\\Windows')
            program_files_root = os.environ.get('PROGRAMFILES', 'C:\\Program Files')
            legacy_path = os.path.join(program_files_root, 'NVIDIA Corporation', 'NVSMI', smi)
            new_path = os.path.join(system_root, 'System32', smi)
            smis = [new_path, legacy_path]
            for candidate_smi in smis:
                if os.path.exists(candidate_smi):
                    smi = '"{}"'.format(candidate_smi)
                    break
        return smi

    def get_cpu_info():
        if sys.platform.startswith('win32'):
            return platform.processor()
        elif sys.platform.startswith('darwin'):
            return subprocess.check_output(['/usr/sbin/sysctl', "-n", "machdep.cpu.brand_string"]).strip()
        elif sys.platform.startswith('linux'):
            command = 'cat /proc/cpuinfo'
            all_info = subprocess.check_output(command, shell=True).decode().strip()
            for line in all_info.split("\n"):
                if 'model name' in line:
                    return re.sub(r'.*model name.*:', '', line, 1)[1:]
        return None

    # --------------------------------------------------------------------------
    #     Gather information
    # --------------------------------------------------------------------------

    if sys.platform.startswith('darwin'):
        nvidia_driver_version = run_and_match('kextstat | grep -i cuda', r'com[.]nvidia[.]CUDA [(](.*?)[)]')
    else:
        nvidia_driver_version = run_and_match(get_nvidia_smi(), r'Driver Version: (.*?) ')

    cuda_version = run_and_match('nvcc --version', r'release .+ V(.*)')
    llvm_version = dr.llvm_version()

    cpu_info = get_cpu_info()
    gpu_info = get_gpu_info()

    is_mitsuba_custom_build = exists(join(dir, '../../../mitsuba.conf'))

    if sys.platform.startswith('darwin'):
        os_version = run_and_match('sw_vers -productVersion', r'(.*)')
    elif sys.platform.startswith('win32'):
        os_version = platform.platform(terse=True)
    else:
        os_version = run_and_match('lsb_release -a', r'Description:\t(.*)')

    python_version = sys.version.replace("\n", "")

    mitsuba_compiler = SourceFileLoader("config", join(dir, '../config.py')).load_module().CXX_COMPILER

    # --------------------------------------------------------------------------
    #     Pretty print
    # --------------------------------------------------------------------------

    def check_print(name, version):
        if version:
            print(f'  {name}: {version}')

    print(f'System information:')
    print(f'')
    check_print('OS', os_version)
    check_print('CPU', cpu_info)
    check_print('GPU', gpu_info)
    check_print('Python', python_version)
    check_print('NVidia driver', nvidia_driver_version)
    check_print('CUDA', cuda_version)
    check_print('LLVM', llvm_version)
    print(f'')
    check_print('Dr.Jit', dr.__version__ + (' (DEBUG)' if dr.DEBUG else ''))
    check_print('Mitsuba', mi.MI_VERSION + (' (DEBUG)' if mi.DEBUG else ''))
    print(f"     Is custom build? {is_mitsuba_custom_build}")
    print(f"     Compiled with: {mitsuba_compiler}")
    print(f'     Variants:')
    for v in mi.variants():
        print(f'        {v}')

