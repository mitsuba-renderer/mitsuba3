How to make a new release?
--------------------------

1. Ensure that the most recent version of Mitsuba is checked out (including all
   submodules).

2. Regenerate the documentation using the `mkoc`, `mkdoc-api` and `docstrings`
   targets and commit the result. Do this with the following command in your
   build folder: ``ninja docstrings && ninja && ninja mkdoc``.
   Do this on a Linux box, with ``docs/requirement.txt`` installed (UV)
   and pandoc installed (apt).

3. Check that the ``nanobind`` dependency version in ``pyroject.toml`` (build
   requirement) matches the version used in the submodule.

4. Update the ``drjit`` dependency version (build requirement and dependency)
   in ``pyroject.toml``, it must match ``ext/drjit/include/drjit/fwd.h``. That
   version of ``drjit`` must also be available on PyPI.

5. Ensure that the changelog is up to date in ``docs/release_notes.rst``.

6. Verify that the CI is currently green on all platforms.

7. Run the GHA "Build Python Wheels" with option "0". This effectively is a dry
   run of the wheel creation process.

8. If the action failed, fix whatever broke in the build process. If it succeded
   continue.

9. Update the version number in ``include/mitsuba/mitsuba.h`` and set
   ``MI_VERSION_DEV`` to an empty string.

10. Add release number and date to ``docs/release_notes.rst``.

12. Update version number in ``README.md``'s and ``docs/index.rst``'s ``bibtex``
    citations.

13. Regenerate the documentation again using the same command:
   ``ninja docstrings && ninja && ninja mkdoc-api mkdoc``.

14. Commit: ``git commit -am "vX.Y.Z release"``

15. Tag: ``git tag -a vX.Y.Z -m "vX.Y.Z release"``

16. Push: ``git push`` and ``git push --tags``

17. Run the GHA "Build Python Wheels" with option "1".

18. Check that the new version is available on
    `readthedocs <https://mitsuba.readthedocs.io/>`__.

19. Create the release on GitHub from the tag pushed at step 16:

    .. code-block:: bash

       python3 resources/changelog_to_release.py X.Y.Z --create

    This converts the matching section of ``docs/release_notes.rst`` to Markdown
    and hands it to ``gh release create``. Omit ``--create`` to preview the
    notes, or add ``--draft`` to review them on GitHub before publishing.

20. Checkout the ``stable`` branch and run ``git pull --ff-only origin vX.Y.Z``
    and ``git push``
