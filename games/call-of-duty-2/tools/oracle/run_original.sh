#!/usr/bin/env bash
# Run the ORIGINAL CoD2 1.3 Linux dedicated server (ELF32) network-isolated and capture reference behaviour.
# Usage: run_original.sh <case> [run-index] [-- extra server args]
#   cases: dm_toujane | sd_carentan | shell (shell = isolated interactive bash, no probe)
# Outputs: .local/cod2/oracle/captures/<case>/run<N>/  (server.log, probe packets, SHA256SUMS)
# Original game data is only read through symlinks (+ LD_PRELOAD shim, see analysis/oracle-baseline.md).
set -euo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO=$(cd "$HERE/../../../.." && pwd)
COD2=${COD2_LOCAL:-$REPO/.local/cod2}
ORACLE=$COD2/oracle
BIN=$COD2/lnxded-1.3/cod2_lnxded
EXPECT_SHA=05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7
DATA=${COD2_DATA_MAIN:?Set_COD2_DATA_MAIN_to_the_original_game_main_directory}
RCON=oracle-test-pw
PORT=28960

CASE=${1:?case}; RUN=${2:-1}; shift $(( $# >= 2 ? 2 : $# )) || true
[ "${1:-}" = "--" ] && shift
EXTRA=("$@")
case "$CASE" in
  dm_toujane)  SRVARGS=(+set g_gametype dm +map mp_toujane) ;;
  sd_carentan) SRVARGS=(+set g_gametype sd +map mp_carentan) ;;
  shell)       SRVARGS=() ;;
  *) echo "unknown case $CASE" >&2; exit 2 ;;
esac

[ "$(sha256sum "$BIN" | cut -d' ' -f1)" = "$EXPECT_SHA" ] || { echo "server binary hash mismatch" >&2; exit 3; }
[ -d "$DATA" ] || { echo "data dir missing: $DATA" >&2; exit 3; }
SHIM=$ORACLE/lib/stat32_shim.so
mkdir -p "$ORACLE/lib" "$ORACLE/run/base/main" "$ORACLE/run/home"
if [ ! -f "$SHIM" ] || [ "$HERE/stat32_shim.c" -nt "$SHIM" ]; then gcc -m32 -shared -fPIC -O2 "$HERE/stat32_shim.c" -o "$SHIM"; fi
for f in "$DATA"/*.iwd; do [ -e "$ORACLE/run/base/main/$(basename "$f")" ] || ln -s "$f" "$ORACLE/run/base/main/"; done
OUT=$ORACLE/captures/$CASE/run$RUN
HOMEDIR=$ORACLE/run/home/$CASE-run$RUN
rm -rf "$OUT" "$HOMEDIR"; mkdir -p "$OUT" "$HOMEDIR"

# Inner script: runs inside the network namespace (loopback only).
INNER=$(mktemp "$ORACLE/run/inner.XXXXXX.sh")
trap 'rm -f "$INNER"' EXIT
cat > "$INNER" <<INNEREOF
#!/usr/bin/env bash
set -u
export LD_LIBRARY_PATH=$COD2/runtime-i386/usr/lib/i386-linux-gnu
cd $COD2/lnxded-1.3
# isolation evidence: interfaces seen by the server namespace
{ echo "# /proc/net/dev"; tail -n +3 /proc/net/dev | cut -d: -f1 | tr -d ' '; echo "# routes"; cat /proc/net/route | tail -n +2; } > $OUT/isolation.txt
if [ "$CASE" = shell ]; then exec bash; fi
mkfifo $OUT/stdin.fifo
sleep 600 > $OUT/stdin.fifo &   # keeps the server's console stdin open
KEEP=\$!
LD_PRELOAD=$SHIM ./cod2_lnxded +set dedicated 1 +set net_ip 127.0.0.1 +set net_port $PORT +set sv_maxclients 8 \\
  +set rcon_password $RCON +set sv_punkbuster 0 +set sv_pure 1 \\
  +set fs_basepath $ORACLE/run/base +set fs_homepath $HOMEDIR \\
  ${SRVARGS[*]} ${EXTRA[*]:-} < $OUT/stdin.fifo > $OUT/server.log 2>&1 &
SRV=\$!
python3 -I $HERE/probe.py capture --port $PORT --rcon $RCON --out $OUT --case $CASE --server-log $OUT/server.log
RC=\$?
for i in \$(seq 1 50); do kill -0 \$SRV 2>/dev/null || break; sleep 0.2; done
if kill -0 \$SRV 2>/dev/null; then echo "server did not exit after rcon quit; killing" >> $OUT/harness.log; kill -9 \$SRV; fi
wait \$SRV 2>/dev/null; echo "server_exit_status=\$?" >> $OUT/harness.log
kill \$KEEP 2>/dev/null; rm -f $OUT/stdin.fifo
exit \$RC
INNEREOF
chmod +x "$INNER"

# --unshare-net: new network namespace, loopback only (bwrap brings lo up). Root fs read-only; only the disposable home dir is writable.
# Original media must be mounted read-only by the operator.
BW=(bwrap --ro-bind / / --dev /dev --proc /proc --bind "$ORACLE/run/home" "$ORACLE/run/home" --bind "$OUT" "$OUT"
    --bind "$ORACLE/run" "$ORACLE/run" --unshare-net --unshare-pid --unshare-ipc --die-with-parent)
"${BW[@]}" bash "$INNER" || echo "probe exit status $?" >> "$OUT/harness.log"
( cd "$OUT" && find . -type f ! -name SHA256SUMS | sort | xargs sha256sum > SHA256SUMS )
echo "done: $OUT"
