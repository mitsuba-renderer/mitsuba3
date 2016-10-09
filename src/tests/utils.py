import pytest

# Fixture to create a temporary file
@pytest.fixture
def tmpfile(request, tmpdir_factory):
    dir = tmpdir_factory.mktemp('tmpdir')
    request.addfinalizer(lambda: dir.remove(rec=1))
    path_value = str(dir.join('tmpfile'))
    open(path_value, 'a').close()
    return path_value
