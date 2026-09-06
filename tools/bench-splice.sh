#!/bin/bash
# bench-splice.sh -- A/B benchmark of mergerfs splice
# against the mergefs OFF baseline and a pristine-HEAD baseline binary.
#
# Run:  bash tools/bench-splice.sh [REPS]
# Env:  BASE_REPO (path to pristine HEAD build dir), SIZE_MIB, REPS
# note: requires `unshare -Urm` permissions for an unprivileged FUSE mount.

# Resolve a working `stat -c %s` once: this host's unshare PATH can
# front a non-GNU stat (aurora) that rejects -c.
STAT_BIN="$(command -v stat)"
if ! "${STAT_BIN}" -c %s /dev/null >/dev/null 2>&1; then
  STAT_BIN=/usr/bin/stat
fi

set -euo pipefail
export LC_ALL=C   # dd prints locale-formatted numbers; parse_mibs needs C

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${REPO}/build/mergerfs"
BASE_REPO="${BASE_REPO:-/tmp/mfs-base}"
BASE_BIN="${BASE_REPO}/build/mergerfs"

B=/dev/shm/bench-splice
REPS="${REPS:-${1:-5}}"

# --- OOM guard: pick SIZE_MIB so mergefs-blob fits comfortably into MemAvailable
mem_available_kib() { awk '/^MemAvailable:/{print $2}' /proc/meminfo; }
pick_size_mib() {
  local want=(256 128 64)
  local w
  for w in "${want[@]}"; do
    local need=$(( w * 2 * 1024 ))   # blob on branch + mergefs page cache
    local have; have=$(mem_available_kib)
    if (( have >= need )); then echo "$w"; return; fi
  done
  echo "INSUFFICIENT-MEMORY" >&2
  return 1
}
SIZE_MIB="${SIZE_MIB:-$(pick_size_mib)}"

# --- helpers -----------------------------------------------------------

die() { echo "bench-splice: $*" >&2; exit 1; }
need_binary() { [[ -x "$1" ]] || die "missing binary: $1"; }

need_splice_binary() {
  # --version comes from src/version.hpp, which incremental makes do not
  # refresh; assert the binary actually contains the code under test.
  # NB: consume strings fully - grep -q closes the pipe early, and with
  # pipefail the resulting SIGPIPE would fail the check spuriously.
  local marker="$2"
  local out
  out=$(strings "$1" | grep "${marker}" || true)
  [[ -n "${out}" ]] \
    || die "binary $1 lacks marker '${marker}' (stale build? run make)"
}

ensure_unshared() {
  # Re-exec under unshare -Urm if we're not already root in a user ns
  # (mount requires CAP_SYS_ADMIN in this host's delegate).
  if (( EUID != 0 )) && [[ -z "${BENCH_INNS:-}" ]]; then
    BENCH_INNS=1 exec unshare -Urm -- "$0" "$@"
  fi
}

# --- mergefs process management -----------------------------------------

declare -g MNT="" BR1="" BR2="" MPID=""

mfs_stop() {
  if [[ -n "${MPID}" ]]; then
    kill "${MPID}" 2>/dev/null || true
    wait "${MPID}" 2>/dev/null || true
    MPID=
  fi
  pkill -9 -f "$BIN .*${B}/mnt" 2>/dev/null || true
  pkill -9 -f "$BASE_BIN .*${B}/mnt" 2>/dev/null || true
  if [[ -n "${MNT}" ]] && grep -qs "${B}/mnt" /proc/mounts; then
    fusermount3 -u "${MNT}" >/dev/null 2>&1 || umount "${MNT}" 2>/dev/null || true
  fi
}

cleanup() {
  mfs_stop
  payload_src_fini
  rm -rf "${B}"
}
trap cleanup EXIT
trap 'cleanup; exit 1' INT TERM

mfs_start() {
  local binary="$1"; shift
  local opts="$1"; shift

  BR1="${B}/b1"; BR2="${B}/b2"; MNT="${B}/mnt"
  mkdir -p "${BR1}" "${BR2}" "${MNT}"

  "${binary}" -f "${BR1}:${BR2}" "${MNT}" -o "${opts}" >/dev/null 2>&1 &
  MPID=$!

  # wait for mount
  local t=0
  while ! grep -qs " ${MNT} " /proc/mounts; do
    sleep 0.2
    t=$(( t + 1 ))
    if (( t > 50 )); then
      die "mount did not come up for ${binary}"
    fi
  done
  sleep 0.5   # let it warm INIT
}

# --- one experiment ------------------------------------------------------

# run_4phases <binary> <mount-opts> <label>
# The four phases: seq_write, seq_read, small_write, small_read
# Writes results rows "label test rep mibs" to $RESULTS.
RESULTS=${RESULTS:-/tmp/bench-splice.$(date +%Y%m%d%H%M%S).results}
: > "${RESULTS}"

parse_mibs() {
  # dd's final stderr line: 'NNN bytes (X biB) copied, T s, R MB/s' where its
  # "MB/s" is decimal. Recompute MiB/s exactly from bytes and seconds.
  tail -n 1 "$1" | awk '
    { for (i = 1; i <= NF; i++) {
        if ($i == "bytes")    bytes = $(i-1)
        if ($i == "copied,")  secs  = $(i+1)
      }
      if (bytes && secs > 0) printf "%.3f", bytes / 1048576.0 / secs
    }'
}

phase_run() {
  local label="$1" test="$2" rep="$3"; shift 3
  local lav tmpf rc
  tmpf=$(mktemp)
  # Explicitly capture dd's exit status: a `;`-joined
  # "dd ...; parse_mibs" (the previous form) ran parse_mibs
  # unconditionally, so a dd that failed partway (e.g. EIO from a
  # splice fallback) could still leave a plausible-looking partial
  # "N bytes ... copied" summary line in ${tmpf} for parse_mibs to
  # extract, recording a corrupt-transport measurement as if it were
  # a valid one instead of aborting the run.
  rc=0
  "$@" 2>"${tmpf}" >/dev/null || rc=$?
  if [[ ${rc} -ne 0 ]]; then
    cat "${tmpf}" >&2
    rm -f "${tmpf}"
    die "dd failed (exit ${rc}) for $label/$test/rep$rep"
  fi
  lav="$(parse_mibs "${tmpf}" || true)"
  rm -f "${tmpf}"
  [[ -n "${lav}" ]] || die "failed to parse MiB/s from dd ($label/$test/rep$rep)"
  echo "$label $test $rep $lav" >> "${RESULTS}"
}

wipe_targets() {
  rm -f "${MNT}/blob" "${BR1}/blob" "${BR2}/blob"
  sync
}
check_integrity() {
  # verify the file on disk matches what's seen through the mount, and
  # that both carry the full expected size (agreement on a truncated
  # blob is NOT integrity).
  # (set -e + pipefail safe: substitutions must never abort the script)
  local expected="$1"
  local mhash bhash msize bsize b_path
  mhash=$(sha256sum "${MNT}/blob" 2>/dev/null | awk '{print $1}') || true
  msize=$("${STAT_BIN}" -c %s "${MNT}/blob" 2>/dev/null) || msize=""
  bhash=""
  bsize=""
  b_path=""
  if [[ -f "${BR1}/blob" ]]; then b_path="${BR1}/blob"; fi
  if [[ -z "${b_path}" && -f "${BR2}/blob" ]]; then b_path="${BR2}/blob"; fi
  if [[ -n "${b_path}" ]]; then
    bhash=$(sha256sum "${b_path}" 2>/dev/null | awk '{print $1}') || true
    bsize=$("${STAT_BIN}" -c %s "${b_path}" 2>/dev/null) || bsize=""
  fi
  [[ -n "${bhash}" ]] || die "blob not found on any branch (integrity)"
  if [[ "${bsize}" != "${expected}" || "${msize}" != "${expected}" ]]; then
    die "blob size ${bsize}/${msize} != expected ${expected} (truncated write, integrity)"
  fi
  if [[ "${mhash}" != "${bhash}" ]]; then
    die "sha256 mismatch branch ${b_path} vs mount (integrity check)"
  fi
}

drop_caches() {
  # Best effort - and normally a no-op here: vm sysctls are not
  # namespaced, so writing drop_caches from inside unshare -Urm
  # requires CAP_SYS_ADMIN in the INITIAL userns and fails with EPERM
  # (masked below). Read phases on tmpfs branches are therefore
  # cache-warm; they measure the transport of cached pages, not cold
  # disk. sync still runs.
  sync
  echo 3 > /proc/sys/vm/drop_caches 2>/dev/null || true
}

# Non-zero payload source: the integrity gate must be able to see
# stale-pool-memory corruption (correct size, wrong bytes). An
# all-zero payload makes stale pool contents indistinguishable from
# the requested data, so the sha256 gate passes green over corrupted
# transport. One urandom file per bench run, reused across phases.
PAYLOAD_SRC=""
payload_src_init() {
  # Sized to the LARGEST payload any arm writes: SIZE_MIB for the
  # main arms, and SIZE_MIB_4K MiB for the 4K arm (RUN_4K defaults
  # on and its size is independent of SIZE_MIB). A short pattern
  # file would make the write dd run short and check_integrity die
  # 'truncated write'. Lives in /dev/shm (same as B) so the read
  # source costs memory speed, not disk.
  local need
  need=$(( SIZE_MIB > ${SIZE_MIB_4K:-64} ? SIZE_MIB : ${SIZE_MIB_4K:-64} ))
  PAYLOAD_SRC=$(mktemp /dev/shm/bench-splice-payload.XXXXXX 2>/dev/null || mktemp /tmp/bench-splice-payload.XXXXXX)
  dd if=/dev/urandom of="${PAYLOAD_SRC}" bs=1M count="${need}" status=none
}
payload_src_fini() {
  # Under `set -e`, "[[ cond ]] && cmd" as a function's only statement
  # returns the (nonzero) test result when cond is false - e.g. when
  # this runs from the EXIT trap before payload_src_init ever set
  # PAYLOAD_SRC (an early die() for a missing/stale binary). That
  # would make cleanup() exit right here and skip its own later
  # `rm -rf "${B}"`, leaking the whole scratch tree. Always return 0.
  [[ -n "${PAYLOAD_SRC}" ]] && rm -f "${PAYLOAD_SRC}"
  return 0
}

# Write (+integrity check) then drop-caches+read one (name,bs,count) pair
# against ${MNT}/blob. Shared by arm_one_rep (seq/small, bs=1M/128K) and
# arm_4k_rep (tiny, bs=4K). NOT used by arm_native: that arm targets the
# raw branch with no mergefs running, so it has nothing to check
# integrity against and no page cache pass-through worth dropping (see
# its own comment) - forcing it through this helper would need those
# checks toggled off by flag, trading a clear structural difference for
# a harder-to-audit boolean.
run_pair() {
  local label="$1" name="$2" bs_arg="$3" count="$4" bytes_per_unit="$5" rep="$6"
  local mnt="${MNT}"

  wipe_targets
  phase_run "${label}" "${name}_write" "${rep}" \
    dd if="${PAYLOAD_SRC}" of="${mnt}/blob" bs="${bs_arg}" count="${count}" conv=fdatasync oflag=nocache
  check_integrity $(( count * bytes_per_unit ))

  drop_caches
  phase_run "${label}" "${name}_read" "${rep}" \
    dd if="${mnt}/blob" of=/dev/null bs="${bs_arg}" iflag=nocache
}

arm_one_rep() {
  local label="$1" rep="$2"

  run_pair "${label}" seq   1M   "${SIZE_MIB}"            1048576 "${rep}"
  run_pair "${label}" small 128K "$(( SIZE_MIB * 8 ))"    131072  "${rep}"

  rm -f "${MNT}/blob"
  sync
}

# Same arm at bs=4K. Uses SIZE_MIB_4K (default 64) so iteration count
# doesn't run 10x the main arm for equal signal. Writes are capped
# proportionally to SIZE_MIB_4K, reads also capped.
arm_4k_rep() {
  local label="$1" rep="$2"
  local sz="${SIZE_MIB_4K:-64}"

  run_pair "${label}_4k" tiny 4K "$(( sz * 256 ))" 4096 "${rep}"

  rm -f "${MNT}/blob"
  sync
}

# native baseline: same 4 dd patterns against raw branch directly, once
arm_native() {
  local b="${BR1}"
  [[ -d "${b}" ]] || die "no branch dir"
  phase_run native seq_write 1 \
    dd if="${PAYLOAD_SRC}" of="${b}/blob" bs=1M count="${SIZE_MIB}" conv=fdatasync oflag=nocache
  phase_run native seq_read 1 \
    dd if="${b}/blob" of=/dev/null bs=1M iflag=nocache
  rm -f "${b}/blob"; sync
  phase_run native small_write 1 \
    dd if="${PAYLOAD_SRC}" of="${b}/blob" bs=128K count=$(( SIZE_MIB * 8 )) conv=fdatasync oflag=nocache
  phase_run native small_read 1 \
    dd if="${b}/blob" of=/dev/null bs=128K iflag=nocache
  rm -f "${b}/blob"; sync
}

# --- main ---------------------------------------------------------------

ensure_unshared "$@"

need_binary "${BIN}"
need_binary "${BASE_BIN}"
need_splice_binary "${BIN}" "reply-fd-splice"

mkdir -p "${B}" "${B}/b1" "${B}/b2" "${B}/mnt"

payload_src_init

# header
{
  echo "== bench-splice =="
  echo "kernel: $(uname -r)"
  echo "binary: $(${BIN} --version 2>/dev/null | head -1 || echo mergerfs)"
  echo "baseline_binary: $(${BASE_BIN} --version 2>/dev/null | head -1 || echo mergerfs)"
  echo "nproc: $(nproc)"
  echo "mem_available_kib: $(mem_available_kib)"
  echo "size_mib: ${SIZE_MIB}"
  echo "reps: ${REPS}"
  echo
} | tee /dev/stderr

# native first (no mergefs running); BR1/BR2/MNT point at bench branches
# before any mergefs is started so arm_native can dd directly.
BR1="${B}/b1"; BR2="${B}/b2"; MNT="${B}/mnt"
arm_native

# baseline binary (HEAD): splice options are meaningless to it
mfs_start "${BASE_BIN}" "defaults,minfreespace=0"
for rep in $(seq 1 "${REPS}"); do
  arm_one_rep baseline "${rep}"
done
mfs_stop

# interleave off/on arms
for rep in $(seq 1 "${REPS}"); do
  mfs_start "${BIN}" "minfreespace=0,splice=false"
  arm_one_rep off "${rep}"
  mfs_stop

  # splice=on (current tree, options ON)
  mfs_start "${BIN}" "minfreespace=0,splice=true"
  arm_one_rep on "${rep}"
  mfs_stop
done

# optional 4K side arm, separate label so rows never aggregate with the
# main on/off pools. Defaults: RUN_4K=1 REPS_4K=3 SIZE_MIB_4K=64.
if [[ "${RUN_4K:-1}" == "1" ]]; then
  for rep in $(seq 1 "${REPS_4K:-3}"); do
    mfs_start "${BIN}" "minfreespace=0,splice=false"
    arm_4k_rep off "${rep}"
    mfs_stop
    mfs_start "${BIN}" "minfreespace=0,splice=true"
    arm_4k_rep on "${rep}"
    mfs_stop
  done
fi

echo
echo "== raw results (label test rep MiB/s) =="
cat "${RESULTS}"
echo

# min/median/max per label x test
echo "== min / median / max per (label,test) =="
awk '
  { x[$1, $2] = x[$1, $2] " " $4 }
  END {
    for (k in x) {
      split(k, kk, SUBSEP)
      n = split(x[k], v, " ")
      # bubble sort small
      for (i = 1; i <= n; i++)
        for (j = i+1; j <= n; j++)
          if (v[i]+0 > v[j]+0) { t=v[i]; v[i]=v[j]; v[j]=t }
      mn = v[1]; mx = v[n]
      if (n % 2 == 1) md = v[(n+1)/2]
      else md = (v[n/2]+0 + v[n/2+1]+0) / 2
      printf "%-10s %-12s n=%d min=%0.1f med=%0.1f max=%0.1f\n", kk[1], kk[2], n, mn, md, mx
    }
  }' "${RESULTS}" | sort -k1,1 -k2,2
echo

echo "== delta table (median of PAIRED per-rep deltas: on vs off, on vs baseline) =="
python3 - "${RESULTS}" <<'PY'
import sys, statistics
from collections import defaultdict
rows=[]
with open(sys.argv[1]) as f:
  for line in f:
    label,test,rep,val=line.split()
    rows.append((label,test,int(rep),float(val)))
# (label,test) -> {rep: val}
by_rep=defaultdict(dict)
for label,test,rep,val in rows:
  by_rep[(label,test)][rep]=val
def med(l,t):
    d=by_rep.get((l,t))
    return statistics.median(d.values()) if d else None
# paired per-rep deltas: join two labels by rep number, take the
# difference only where BOTH arms have that rep. The bench interleaves
# off/on per rep precisely so slow drift in host load cancels within a
# pair; pooled medians discard that pairing and were measured 20%+ off
# on this shared host for identical code.
def paired_deltas(a,b,t):
    da=by_rep.get((a,t),{}); db=by_rep.get((b,t),{})
    common=sorted(set(da)&set(db))
    return [da[r]-db[r] for r in common], len(common)
tests=["seq_write","seq_read","small_write","small_read"]
print(f"{'test':<12} {'native':>10} {'baseline':>10} {'off':>10} {'on':>10}  {'on/off':>7} {'on/base':>7}  pairs")
for t in tests:
    n=med("native",t); b=med("baseline",t); o=med("off",t); on=med("on",t)
    d_on_off,no_=paired_deltas("on","off",t)
    d_on_base,nb=paired_deltas("on","baseline",t)
    def pctp(d,base):
        # per-column guard: on/base must not depend on the off arm
        if not d or not base: return "  n/a  "
        return f"{100.0*statistics.median(d)/base:+.1f}%"
    p1=pctp(d_on_off,o)
    p2=pctp(d_on_base,b)
    print(f"{t:<12} {n if n else 0:>10.1f} {b if b else 0:>10.1f} {o if o else 0:>10.1f} {on if on else 0:>10.1f}  {p1:>7} {p2:>7}  {no_}/{nb}")
PY
