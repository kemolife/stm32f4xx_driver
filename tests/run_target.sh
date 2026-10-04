#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# Builds the on-target test image, flashes it to the NUCLEO-F446RE, runs it and
# prints the result. Same as: make test
#
#   tests/run_target.sh              run with the default timeout (60 s)
#   TIMEOUT_S=120 tests/run_target.sh
#
# Exit code: 0 all passed (skips allowed), 1 test failures,
#            2 tests did not finish (hang), 3 build / flash / tool error
#
# Needs: make, arm-none-eabi-gcc, openocd, python3 (to decode the SWO log),
# board connected over USB (ST-LINK).
# -----------------------------------------------------------------------------
set -u
cd "$(dirname "$0")/.."

ELF=build/tests.elf
SWO_LOG=build/tests_swo.bin
TIMEOUT_S=${TIMEOUT_S:-60}

make tests >/dev/null || { echo "build failed (run 'make tests' to see why)"; exit 3; }

SUMMARY=$(arm-none-eabi-nm "$ELF" | awk '$3 == "g_test_summary" { print "0x" $1 }')
if [ -z "$SUMMARY" ]; then
	echo "g_test_summary not found in $ELF"
	exit 3
fi

rm -f "$SWO_LOG"
echo "flashing and running tests (timeout ${TIMEOUT_S} s) ..."

OUT=$(openocd -f board/st_nucleo_f4.cfg \
	-c "set ELF $ELF" -c "set SUMMARY $SUMMARY" \
	-c "set TIMEOUT_S $TIMEOUT_S" -c "set SWO_LOG $SWO_LOG" \
	-f tests/run_target.tcl 2>&1)
STATUS=$?

RESULT=$(echo "$OUT" | grep '^RESULT ')
if [ $STATUS -ne 0 ] || [ -z "$RESULT" ]; then
	echo "$OUT" | tail -20
	echo "openocd failed: board connected? (exit $STATUS)"
	exit 3
fi
echo "$OUT" | grep '^NOTE' || true

# Decode the SWO stream: ITM software packets on port 0 are the printf output.
# Header byte: bits [1:0] = payload size (1, 2, 4 bytes), bits [7:3] = port.
if [ -s "$SWO_LOG" ]; then
	echo "----- test output (SWO) -----"
	python3 - "$SWO_LOG" <<'EOF'
import sys
data = open(sys.argv[1], "rb").read()
out, i = bytearray(), 0
while i < len(data):
    h = data[i]; i += 1
    size = {1: 1, 2: 2, 3: 4}.get(h & 0x03, 0)
    if size and not (h & 0x04):          # software (ITM stimulus) packet
        payload = data[i:i + size]; i += size
        if (h >> 3) == 0:
            out += payload
    # sync (0x00) and other protocol packets carry no text
sys.stdout.write(out.decode("ascii", "replace"))
EOF
	echo "-----------------------------"
fi

read -r _ DONE RUN PASSED FAILED SKIPPED CHECKS <<< "$RESULT"
CURRENT=$(echo "$OUT" | sed -n 's/^CURRENT //p')
FIRSTFAIL=$(echo "$OUT" | sed -n 's/^FIRSTFAIL //p')

if [ "$DONE" != "1" ]; then
	echo "NOT FINISHED after ${TIMEOUT_S} s: stuck in test '$CURRENT' ($RUN tests started)"
	exit 2
fi

echo "$RUN tests: $PASSED passed, $FAILED failed, $SKIPPED skipped ($CHECKS failed checks)"
if [ "$FAILED" != "0" ]; then
	echo "first failure: $FIRSTFAIL"
	exit 1
fi
exit 0
