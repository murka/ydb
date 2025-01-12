import library.python.svn_version as sv


def test_simple():
    assert sv.svn_version()
    assert isinstance(sv.svn_version(), str)

    # svn_revision() will be equal to zero on non-trunk commits
    assert sv.svn_revision() >= 0
    assert isinstance(sv.svn_revision(), int)

    # svn_last_revision() will be equal to zero on non-trunk commits
    assert sv.svn_last_revision() >= 0
    assert isinstance(sv.svn_last_revision(), int)

    assert sv.commit_id()
    assert isinstance(sv.commit_id(), str)
    assert len(sv.commit_id()) > 0
    assert isinstance(sv.hash(), str)
    assert isinstance(sv.patch_number(), int)
