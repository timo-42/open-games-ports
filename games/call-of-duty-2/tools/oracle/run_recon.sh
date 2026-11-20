#!/usr/bin/env bash
# Run the RECONSTRUCTED cod2_lnxded (.local/cod2/build/cod2_lnxded_recon) exactly like the oracle
# (run_original.sh): same sandbox (bwrap --unshare-net, loopback only), same server args, same
# fs_basepath/fs_homepath layout, same stat32 shim, so server.log is directly diffable.
# Usage: run_recon.sh <mode> <case> [run-index] [-- extra server args]
#   mode: capture  -> probe sequence like the oracle, outputs .local/cod2/recon-run/captures/<case>/run<N>/
#         gdb      -> batch gdb (commands from $GDB_CMDS, default: run + bt), output in .../gdb/<case>/run<N>/
#         plain    -> just run the server (stdin from a fifo kept open $HOLD seconds, default 20), log only
#   cases: dm_toujane | sd_carentan | none (no +map)
# Never run the reconstruction outside this sandbox (no LAN exposure).
set -euo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO=$(cd "$HERE/../../../.." && pwd)
COD2=${COD2_LOCAL:-$REPO/.local/cod2}
ORACLE=$COD2/oracle
BIN=${RECON_BIN:-$COD2/build/cod2_lnxded_recon}
DATA=${COD2_DATA_MAIN:?Set_COD2_DATA_MAIN_to_the_original_game_main_directory}
RCON=oracle-test-pw
PORT=28960
HOLD=${HOLD:-20}

MODE=${1:?mode}; CASE=${2:?case}; RUN=${3:-1}; shift $(( $# >= 3 ? 3 : $# )) || true
[ "${1:-}" = "--" ] && shift
EXTRA=("$@")
case "$CASE" in
  dm_toujane)  SRVARGS=(+set g_gametype dm +map mp_toujane) ;;
  sd_carentan) SRVARGS=(+set g_gametype sd +map mp_carentan) ;;
  none)        SRVARGS=() ;;
  *) echo "unknown case $CASE" >&2; exit 2 ;;
esac
[ -x "$BIN" ] || { echo "missing $BIN (run tools/link_test.py)" >&2; exit 3; }
[ -d "$DATA" ] || { echo "data dir missing: $DATA" >&2; exit 3; }
SHIM=$ORACLE/lib/stat32_shim.so
mkdir -p "$ORACLE/lib" "$ORACLE/run/base/main" "$ORACLE/run/home"
if [ ! -f "$SHIM" ] || [ "$HERE/stat32_shim.c" -nt "$SHIM" ]; then gcc -m32 -shared -fPIC -O2 "$HERE/stat32_shim.c" -o "$SHIM"; fi
for f in "$DATA"/*.iwd; do [ -e "$ORACLE/run/base/main/$(basename "$f")" ] || ln -s "$f" "$ORACLE/run/base/main/"; done
OUT=$COD2/recon-run/$MODE/$CASE/run$RUN
HOMEDIR=$ORACLE/run/home/$CASE-run$RUN
rm -rf "$OUT" "$HOMEDIR"; mkdir -p "$OUT" "$HOMEDIR"
GDB_CMDS=${GDB_CMDS:-}
if [ "$MODE" = gdb ] && [ -z "$GDB_CMDS" ]; then
  GDB_CMDS=$OUT/default.gdb
  printf 'set pagination off\nset confirm off\nhandle SIGPIPE nostop noprint\nrun\nbt 40\ninfo registers\nx/16i $pc-24\n' > "$GDB_CMDS"
fi

INNER=$(mktemp "$ORACLE/run/inner.XXXXXX.sh")
trap 'rm -f "$INNER"' EXIT
cat > "$INNER" <<INNEREOF
#!/usr/bin/env bash
set -u
cd $COD2/lnxded-1.3
ARGS=(+set dedicated 1 +set net_ip 127.0.0.1 +set net_port $PORT +set sv_maxclients 8
  +set rcon_password $RCON +set sv_punkbuster 0 +set sv_pure 1
  +set fs_basepath $ORACLE/run/base +set fs_homepath $HOMEDIR ${SRVARGS[*]} ${EXTRA[*]:-})
mkfifo $OUT/stdin.fifo
if [ "$MODE" = capture ]; then sleep 600 > $OUT/stdin.fifo & else sleep $HOLD > $OUT/stdin.fifo & fi
KEEP=\$!
if [ "$MODE" = gdb ]; then
  # watchdog: interrupt the inferior after TLIMIT s so the gdb script also prints a backtrace of a hang
  ( sleep ${TLIMIT:-60}; pkill -INT -f "^$BIN" ) &
  gdb -q -batch -x $GDB_CMDS --args env LD_PRELOAD=$SHIM $BIN "\${ARGS[@]}" < $OUT/stdin.fifo > $OUT/gdb.log 2>&1
  echo "gdb_exit_status=\$?" >> $OUT/harness.log
  kill \$KEEP 2>/dev/null; rm -f $OUT/stdin.fifo; exit 0
fi
LD_PRELOAD=$SHIM $BIN "\${ARGS[@]}" < $OUT/stdin.fifo > $OUT/server.log 2>&1 &
SRV=\$!
RC=0
if [ "$MODE" = capture ]; then
  python3 -I $HERE/probe.py capture --port $PORT --rcon $RCON --out $OUT --case $CASE --server-log $OUT/server.log
  RC=\$?
  for i in \$(seq 1 50); do kill -0 \$SRV 2>/dev/null || break; sleep 0.2; done
fi
if [ "$MODE" = plain ]; then
  for i in \$(seq 1 \$((HOLD*5))); do kill -0 \$SRV 2>/dev/null || break; sleep 0.2; done
fi
if kill -0 \$SRV 2>/dev/null; then echo "server still running; killing" >> $OUT/harness.log; kill -9 \$SRV; fi
wait \$SRV 2>/dev/null; echo "server_exit_status=\$?" >> $OUT/harness.log
kill \$KEEP 2>/dev/null; rm -f $OUT/stdin.fifo
exit \$RC
INNEREOF
chmod +x "$INNER"
BW=(bwrap --ro-bind / / --dev /dev --proc /proc --bind "$ORACLE/run" "$ORACLE/run" --bind "$OUT" "$OUT"
    --unshare-net --unshare-pid --unshare-ipc --die-with-parent)
"${BW[@]}" bash "$INNER" || echo "inner exit status $?" >> "$OUT/harness.log"
echo "done: $OUT"
