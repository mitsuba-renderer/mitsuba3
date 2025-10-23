import pytest
import drjit as dr
import mitsuba as mi
import numpy as np


def get_increment_sampler_class():
    class IncrementSampler(mi.Sampler):
        """
        A deterministic sampler that returns values in 0.1 increments.

        Values cycle through: 0.0, 0.1, 0.2, ..., 0.9, then wrap back to 0.0.
        The seed value is used as an offset by dividing it by 10 until it's less than 0.1.
        """

        def __init__(self, props=mi.Properties()):
            super().__init__(props)
            self.m_sample_count = props.get('sample_count', 4)
            self.m_base_seed = props.get('seed', 0)
            self.m_offset = mi.Float(0.0)
            self.m_dimension_counter = mi.UInt32(0)
            self._process_seed(self.m_base_seed)

        def _process_seed(self, seed):
            offset = dr.slice(seed)
            while offset >= 0.1:
                offset /= 10.0
            self.m_offset = mi.Float(offset)

        def seed(self, seed, wavefront_size=(2**32-1)):
            super().seed(seed, wavefront_size)
            self.m_base_seed = int(seed) if hasattr(seed, '__int__') else seed
            self._process_seed(self.m_base_seed)
            self.m_dimension_counter = mi.UInt32(0)

        def advance(self):
            super().advance()
            self.m_dimension_counter = mi.UInt32(0)

        def next_1d(self, active=True):
            # Calculate value based on counter and offset
            value = ((self.m_dimension_counter % 10) * 0.1 + self.m_offset)
            self.m_dimension_counter += 1

            # Handle vectorized case
            if dr.is_array_v(active):
                return dr.select(active, value, 0.0)
            else:
                return value if active else 0.0

        def next_2d(self, active=True):
            x = self.next_1d(active)
            y = self.next_1d(active)
            return mi.Point2f(x, y)

        def clone(self):
            sampler = IncrementSampler(mi.Properties())
            sampler.m_sample_count = self.m_sample_count
            sampler.m_base_seed = self.m_base_seed
            sampler.m_offset = self.m_offset
            sampler.m_dimension_counter = self.m_dimension_counter
            sampler.m_wavefront_size = self.m_wavefront_size
            return sampler

        def fork(self):
            sampler = IncrementSampler(mi.Properties())
            sampler.m_sample_count = self.m_sample_count
            sampler.m_base_seed = self.m_base_seed
            return sampler

        def __repr__(self):
            return (f'IncrementSampler[sample_count={self.m_sample_count}' +
                    f', seed={self.m_base_seed}, offset={self.m_offset:.3f}]')

    return IncrementSampler


def setup_increment_sampler():
    """Register the increment sampler for the current variant."""
    IncrementSampler = get_increment_sampler_class()
    mi.register_sampler('increment', lambda props: IncrementSampler(props))


def test01_construct(variants_vec_backends_once_rgb):
    """Test basic construction and property access."""
    setup_increment_sampler()

    sampler = mi.load_dict({
        "type": "increment",
        "sample_count": 10
    })
    assert sampler is not None
    assert sampler.sample_count() == 10


def test02_basic_1d_sampling(variants_vec_backends_once_rgb):
    """Test basic 1D sampling with 20+ calls verifying increment pattern."""
    setup_increment_sampler()

    sampler = mi.load_dict({
        "type": "increment",
        "sample_count": 25
    })
    sampler.seed(0, 1)

    expected_values = [
        0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9,
        0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9,
        0.0, 0.1, 0.2, 0.3, 0.4
    ]

    for _, expected in enumerate(expected_values):
        value = sampler.next_1d()
        assert dr.allclose(value, expected)


def test03_2d_sampling(variants_vec_backends_once_rgb):
    """Test 2D sampling behavior."""
    setup_increment_sampler()

    sampler = mi.load_dict({
        "type": "increment",
        "sample_count": 10
    })
    sampler.seed(0, 1)

    # Test 2D sampling
    point = sampler.next_2d()
    assert dr.allclose(point, (0.0, 0.1))
    point = sampler.next_2d()
    assert dr.allclose(point, (0.2, 0.3))

    # Test advance() resets dimension counter
    sampler.advance()
    point = sampler.next_2d()
    assert dr.allclose(point, (0.0, 0.1))


def test04_seed(variants_vec_backends_once_rgb):
    """Test seed offset calculation with various seed values."""
    setup_increment_sampler()

    test_cases = [
        (0, 0.0),
        (5, 0.05),
        (123, 0.0123),
        (1000, 0.01),
        (999, 0.0999),
    ]

    for seed, expected_offset in test_cases:
        sampler = mi.load_dict({
            "type": "increment",
            "sample_count": 5
        })
        sampler.seed(seed, 1)

        # First value should be the offset
        first_value = sampler.next_1d()
        assert dr.allclose(first_value, expected_offset)

        # Second value should be offset + 0.1
        second_value = sampler.next_1d()
        expected_second = (expected_offset + 0.1) % 1.0
        assert dr.allclose(second_value, expected_second)


def test05_loop(variants_vec_backends_once_rgb):
    """Test sampler with Dr.Jit while loop calling it 25 times."""
    setup_increment_sampler()

    sampler = mi.load_dict({
        "type": "increment",
        "sample_count": 25
    })
    sampler.seed(0, 1)

    @dr.syntax
    def sample_loop(sampler):
        i = mi.UInt32(0)
        sum_val = mi.Float(0.0)

        while i < 20:
            value = sampler.next_1d()
            sum_val += value
            i += 1

        return sum_val

    total = sample_loop(sampler)

    expected_sum = 9.0
    assert dr.allclose(total, expected_sum)


def test12_xml_configuration(variants_vec_backends_once_rgb):
    """Test sampler can be loaded from XML string configuration."""
    setup_increment_sampler()

    xml_config = """
    <sampler type="increment" version="3.0.0">
        <integer name="sample_count" value="16"/>
    </sampler>
    """

    sampler = mi.load_string(xml_config)
    assert sampler is not None
    assert sampler.sample_count() == 16
