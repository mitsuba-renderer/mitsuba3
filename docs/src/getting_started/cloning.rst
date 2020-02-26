.. _sec-cloning:

Cloning the repository
======================

Mitsuba depends on several external dependencies, and its repository directly
refers to specific versions of them using a Git feature called *submodules*.
Cloning Mitsuba's repository will recursively fetch these dependencies, which
are subsequently compiled using a single unified build system. This
dramatically reduces the number steps needed to set up the renderer compared to
previous versions of Mitsuba.

For all of this to work out properly, you will have to specify the
``--recursive`` flag when cloning the repository:

.. code-block:: bash

    git clone --recursive https://github.com/mitsuba-renderer/mitsuba2

If you already cloned the repository and forgot to specify this flag, it's
possible to fix the repository in retrospect using the following command:

.. code-block:: bash

    git submodule update --init --recursive

Staying up-to-date
------------------

Unfortunately, pulling from the main repository won't automatically keep the
submodules in sync, which can lead to various problems. The following command
installs a git alias named ``pullall`` that automates these two steps.

.. code-block:: bash

    git config --global alias.pullall '!f(){ git pull "$@" && git submodule update --init --recursive; }; f'

Afterwards, simply write

.. code-block:: bash

    git pullall

to fetch the latest version of Mitsuba 2.
