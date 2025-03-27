import pytest
import mitsuba as mi
import drjit as dr

def create_tape(wav_bins=1, time_bins=1, count=False):
    tape_dict = {
        "type": "tape",
        "wav_bins": wav_bins,#FIXME: implement list of values
        "time_bins": time_bins,
        "count": count,
    }
    return mi.load_dict(tape_dict)


wav_bins_list = [1, 2, 4]
time_bins_list = [1, 2, 3, 100]


@pytest.mark.parametrize("wav_bins", wav_bins_list)
@pytest.mark.parametrize("time_bins", time_bins_list)
def test01_construct(variants_any_acoustic, wav_bins, time_bins):
    tape = create_tape(wav_bins=wav_bins, time_bins=time_bins)


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

@pytest.mark.parametrize("count", count_list)
def test03_prepare_sample(variants_any_acoustic, count):
    tape = create_tape(wav_bins=1, time_bins=1, count=count)
    tape.prepare([])
    
    channels = tape.base_channels_count()
    assert channels == 2 if count else 1
    
    
    sample = mi.Spectrum([0.5])
    prepared_sample = tape.prepare_sample(sample, wavelengths=mi.Spectrum(1.0), nChannels=channels)
    
    assert dr.allclose(prepared_sample[0], sample)
    if count:
        assert dr.allclose(prepared_sample[1], mi.Spectrum(1.0))


def test04_prepare_sample_spectral(variants_all_spectral):
    tape = create_tape(wav_bins=1, time_bins=1)
    
    tape.prepare([])
    
    with pytest.raises(Exception):
        tape.prepare_sample([])