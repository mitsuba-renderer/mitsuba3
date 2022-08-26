# -*- coding: utf-8 -*-

from setuptools import setup

setup(
    name="extra_mitsuba_plugins",
    version="1.0",
    author="John Doe",
    author_email="john.doe@domain.com",
    description="Example package of extra plugins entry point",
    license="BSD",
    install_requires=[f"mitsuba"],
    packages=['extra_mitsuba_plugins'],
    entry_points={
        'mitsuba.plugins': [
            'tinteddielectric = extra_mitsuba_plugins:TintedDielectric',
            'weirddielectric = extra_mitsuba_plugins:WeirdDielectric',
        ]
    },
    python_requires=">=3.8"
)
