#!/usr/bin/env python3

import ctypes
import errno
import os
import shutil
import stat
import tempfile


libc = ctypes.CDLL(None, use_errno=True)
libc.access.argtypes = [ctypes.c_char_p, ctypes.c_int]
libc.access.restype = ctypes.c_int


def temp_dir(root):
    """Create a temporary directory within the given root directory."""
    return tempfile.mkdtemp(dir=root)


def cleanup_dir(path):
    """Recursively remove a directory and its contents, ignoring errors."""
    if path and os.path.exists(path):
        try:
            shutil.rmtree(path, ignore_errors=True)
        except Exception:
            pass


def invoke(callable_):
    try:
        return True, callable_(), 0
    except OSError as exc:
        return False, None, exc.errno


def compare_calls(name, merge_call, native_call, value_cmp=None, close_fds=False):
    m_ok, m_val, m_errno = invoke(merge_call)
    n_ok, n_val, n_errno = invoke(native_call)

    if close_fds:
        # WARNING: close_fds=True should only be used when the callable
        # returns an fd (non-negative int from os.open etc.), NOT when it
        # returns arbitrary integers (e.g., byte counts, offsets).
        try:
            if m_ok:
                close_if_fd(m_val)
        finally:
            if n_ok:
                close_if_fd(n_val)

    if m_ok != n_ok:
        return (
            f"{name}: success mismatch mergerfs={m_ok} native={n_ok} "
            f"(mergerfs_errno={m_errno} native_errno={n_errno})"
        )
    if m_errno != n_errno:
        return f"{name}: errno mismatch mergerfs={m_errno} native={n_errno}"
    if m_ok and value_cmp is not None and not value_cmp(m_val, n_val):
        return f"{name}: value mismatch mergerfs={m_val!r} native={n_val!r}"

    return None


def access_raw(path, mode):
    ctypes.set_errno(0)
    rv = libc.access(path.encode(), mode)
    err = ctypes.get_errno()
    return rv, err


def compare_access(name, merge_path, native_path, mode, expect_errno=None):
    m_rv, m_errno = access_raw(merge_path, mode)
    n_rv, n_errno = access_raw(native_path, mode)

    if m_rv != n_rv:
        return f"{name}: return mismatch mergerfs={m_rv} native={n_rv}"
    if m_errno != n_errno:
        return f"{name}: errno mismatch mergerfs={m_errno} native={n_errno}"
    if expect_errno is not None and n_errno != expect_errno:
        return f"{name}: expected errno={expect_errno}, got errno={n_errno}"

    return None


def stat_cmp_basic(lhs, rhs):
    return (
        stat.S_IFMT(lhs.st_mode) == stat.S_IFMT(rhs.st_mode)
        and stat.S_IMODE(lhs.st_mode) == stat.S_IMODE(rhs.st_mode)
        and lhs.st_size == rhs.st_size
    )


def cleanup_paths(paths):
    for path in paths:
        try:
            if os.path.islink(path) or os.path.isfile(path):
                os.unlink(path)
            elif os.path.isdir(path):
                os.rmdir(path)
        except FileNotFoundError:
            pass


def close_if_fd(value):
    if isinstance(value, int) and value >= 0:
        try:
            os.close(value)
        except OSError:
            pass


def fail(msg):
    print(msg, end="")
    return 1


def join(root, rel):
    return os.path.join(root, rel)


def ensure_parent(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)


def touch(path, data=b"x", mode=0o644):
    ensure_parent(path)
    with open(path, "wb") as fp:
        fp.write(data)
    os.chmod(path, mode)


def mergerfs_ctrl_file(mount):
    return join(mount, ".mergerfs")


def mergerfs_key(option_key):
    return f"user.mergerfs.{option_key}"


def mergerfs_get_option(mount, option_key):
    raw = os.getxattr(mergerfs_ctrl_file(mount), mergerfs_key(option_key))
    return raw.decode("utf-8", errors="surrogateescape")


def mergerfs_set_option(mount, option_key, value):
    if isinstance(value, bytes):
        payload = value
    else:
        payload = str(value).encode("utf-8")
    os.setxattr(mergerfs_ctrl_file(mount), mergerfs_key(option_key), payload)


def parse_allpaths(raw):
    if isinstance(raw, str):
        raw = raw.encode("utf-8", errors="surrogateescape")
    paths = []
    for p in raw.split(b"\0"):
        if not p:
            continue
        paths.append(p.decode("utf-8", errors="surrogateescape"))
    return paths


def _parse_branch_entry(entry):
    entry = entry.strip()
    if not entry:
        return None
    if "=" in entry:
        return entry.split("=", 1)[0]
    return entry


def mergerfs_branches(mount):
    raw = mergerfs_get_option(mount, "branches")
    branches = []
    for entry in raw.split(":"):
        path = _parse_branch_entry(entry)
        if path:
            branches.append(path)
    return branches


def underlying_path(mount, rel):
    branches = mergerfs_branches(mount)
    if not branches:
        raise RuntimeError("no mergerfs branches configured")
    return os.path.join(branches[0], rel)


def pair_paths(mount, rel):
    return join(mount, rel), underlying_path(mount, rel)


def should_compare_inode(mount):
    try:
        inodecalc = mergerfs_get_option(mount, "inodecalc").strip().lower()
    except OSError:
        return False
    return inodecalc == "passthrough"


def mergerfs_fullpath(path):
    raw = os.getxattr(path, "user.mergerfs.fullpath")
    return raw.decode("utf-8", errors="surrogateescape")
