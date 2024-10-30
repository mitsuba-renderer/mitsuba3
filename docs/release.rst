How to make a new release?
--------------------------

1. Ensure that the most recent version of Mitsuba is checked out (including all
   submodules).

2. Regenerate the documentation using the `mkoc`, `mkdoc-api` and `docstrings`
   targets and commit the result. Do this with the following command in your
   build folder: ``ninja docstrings && ninja && ninja mkdoc-api mkdoc``.

3. Check that the ``nanobind`` dependency version in ``pyroject.toml`` matches
   the version used in the submodule.

4. Update the ``drjit`` dependency version (build requirement and dependency)
   in ``pyroject.toml``, it must match ``ext/drjit/include/drjit/fwd.h``. That
   version of ``drjit`` must also be available on PyPI.

5. Ensure that the changelog is up to date in ``docs/release_notes.rst``.

6. Verify that the CI is currently green all platforms.

7. Run the GHA "Build Python Wheels" with option "0". This effectively is a dry
   run of the wheel creation process.

8. If the action failed, fix whatever broke in the build process. If it succeded
   continue.

9. Update the version number in ``include/mitsuba/mitsuba.h``.

10. Add release number and date to ``docs/release_notes.rst``.

11. Commit: ``git commit -am "vX.Y.Z release"``

12. Tag: ``git tag -a vX.Y.Z -m "vX.Y.Z release"``

13. Push: ``git push`` and ``git push --tags``

14. Run the GHA "Build Python Wheels" with option "1".

15. Check that the new version is available on
    `readthedocs <https://mitsuba.readthedocs.io/>`__.

16. Create a `release on GitHub <https://github.com/mitsuba-renderer/mitsuba3/releases/new>`__
    from the tag created at step 10. The changelog can be copied from step 2.

17. Checkout the ``stable`` branch and run ``git pull --ff-only origin vX.Y.Z``
    and ``git push``
