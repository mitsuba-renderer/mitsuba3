import pytest


@pytest.fixture
def tmpfile(request, tmpdir_factory):
    """Fixture to create a temporary file"""
    my_dir = tmpdir_factory.mktemp('tmpdir')
    request.addfinalizer(lambda: my_dir.remove(rec=1))
    path_value = str(my_dir.join('tmpfile'))
    open(path_value, 'a').close()
    return path_value
