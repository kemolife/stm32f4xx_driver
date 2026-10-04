# -----------------------------------------------------------------------------
# openocd script used by tests/run_target.sh. Do not run it alone.
#
# Expects these variables, set with -c before this file:
#   ELF        test image to flash
#   SUMMARY    address of g_test_summary (from arm-none-eabi-nm)
#   TIMEOUT_S  how long the tests may run
#   SWO_LOG    file for the raw SWO stream (ITM printf output)
#
# Prints machine-readable lines for the shell script:
#   RESULT <done> <run> <passed> <failed> <skipped> <checks_failed>
#   CURRENT <test name>
#   FIRSTFAIL <file>:<line>
# -----------------------------------------------------------------------------

init

# Flash and verify. The target stays halted afterwards.
program $ELF verify

# Capture ITM port 0 (printf) over SWO: core clock 16 MHz (HSI after reset),
# 2 MHz SWO. Optional: if it fails, only the summary is reported.
if {[catch {
	stm32f4x.tpiu configure -protocol uart -traceclk 16000000 -pin-freq 2000000 -output $SWO_LOG
	stm32f4x.tpiu enable
	itm port 0 on
} err]} {
	echo "NOTE: SWO capture not available ($err)"
}

reset run

# TestSummary_t is 9 words: run, passed, failed, skipped, checks_failed,
# first_fail_file (ptr), first_fail_line, current_test (ptr), done (low byte)
proc read_summary {addr} {
	# ST-LINK can read memory while the core runs. If this openocd cannot,
	# halt for the read and resume at once.
	if {[catch {set w [read_memory $addr 32 9]}]} {
		halt
		set w [read_memory $addr 32 9]
		resume
	}
	return $w
}

proc cstr {addr} {
	if {$addr == 0} { return "" }
	set s ""
	foreach b [read_memory $addr 8 80] {
		if {$b == 0} { break }
		append s [format %c $b]
	}
	return $s
}

set start [clock seconds]
set w [read_summary $SUMMARY]
while {(([lindex $w 8] & 0xFF) == 0) && (([clock seconds] - $start) < $TIMEOUT_S)} {
	sleep 250
	set w [read_summary $SUMMARY]
}

# Halt for the final reads, so the strings are stable
halt
set w [read_memory $SUMMARY 32 9]
set done [expr {[lindex $w 8] & 0xFF}]

echo "RESULT $done [lindex $w 0] [lindex $w 1] [lindex $w 2] [lindex $w 3] [lindex $w 4]"
echo "CURRENT [cstr [lindex $w 7]]"
echo "FIRSTFAIL [file tail [cstr [lindex $w 5]]]:[lindex $w 6]"

resume
shutdown
