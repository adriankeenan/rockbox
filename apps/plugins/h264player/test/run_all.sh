#!/bin/sh
# Everything that can be checked without a device.
HERE=$(cd "$(dirname "$0")" && pwd)
rc=0
"$HERE/run_tests.sh" || rc=1
for t in ${SIM_TARGETS:-erosqnative rg35xxpro}; do
    "$HERE/run_sim.sh" "$t" || rc=1
done
[ $rc = 0 ] && echo "RUN_ALL PASS" || echo "RUN_ALL FAIL"
exit $rc
