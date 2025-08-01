import pytest
import mitsuba as mi
import drjit as dr
mi.set_log_level(mi.LogLevel.Trace)

def create_tape(freq_bins=1, time_bins=1, count=False):
    
    freq_string = ', '.join(str(125*2**(i)) for i in range(freq_bins)) # Example frequencies: 125, 250, 500, ...
    tape_dict = {
        "type": "tape",
        "frequencies": freq_string,  
        "time_bins": time_bins,
        "count": count,
    }
    return mi.load_dict(tape_dict)

freq_bins_list = [1, 2, 4]
time_bins_list = [1, 100]


@pytest.mark.parametrize("freq_bins", freq_bins_list)
@pytest.mark.parametrize("time_bins", time_bins_list)
def test01_construct(variants_any_acoustic, freq_bins, time_bins):
    tape = create_tape(freq_bins=freq_bins, time_bins=time_bins)
    print(tape)


count_list = [False, True]

@pytest.mark.parametrize("count", count_list)
def test02_prepare(variants_any_acoustic, count):
    tape = create_tape(count=count)
    tape.prepare([])

    total_channels = 2 if count else 1

    assert tape.base_channels_count() == total_channels

    # run again to check if it resets the tape
    tape.prepare([])
    assert tape.base_channels_count() == total_channels
    print(tape)

@pytest.mark.parametrize("count", count_list)
def test03_prepare_sample(variants_any_acoustic, count):
    tape = create_tape(freq_bins=1, time_bins=1, count=count)
    tape.prepare([])
    
    channels = tape.base_channels_count()
    assert channels == 2 if count else 1
    
    sample = mi.Spectrum([0.5])
    prepared_sample = tape.prepare_sample(sample, wavelengths=mi.Spectrum(1.0), nChannels=channels)
    print(f'sample: {sample}, count: {count}, prepared sample: {prepared_sample}')
    assert dr.allclose(prepared_sample[0], sample)
    if count:
        assert dr.allclose(prepared_sample[1], mi.Spectrum(1.0))