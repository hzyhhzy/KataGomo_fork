#!/bin/bash -eu
set -o pipefail
{
#Shuffles and copies selfplay training from selfplay/ to shuffleddata/current/
#Should be run periodically.

if [[ $# -lt 2 ]]
then
    echo "Usage: $0 BASEDIR TMPDIR NTHREADS BATCHSIZE"
    echo "Currently expects to be run from within the 'python' directory of the KataGo repo, or otherwise in the same dir as shuffle.py."
    echo "BASEDIR containing selfplay data and models and related directories"
    echo "TMPDIR scratch space, ideally on fast local disk, unique to this loop"
    echo "NTHREADS number of parallel threads/processes to use in shuffle"
    echo "BATCHSIZE number of samples to concat together per batch for training"
    exit 0
fi
BASEDIR="$1"
shift
NTHREADS="$1"
shift

#------------------------------------------------------------------------------



echo "Beginning counting data at" $(date "+%Y-%m-%d %H:%M:%S")

#set -x
(
    time python ./count_data_and_wait.py \
         "$BASEDIR"/selfplay/ \
         -expand-window-per-row 0.3 \
         -taper-window-exponent 0.8 \
         -num-processes "$NTHREADS" \
         -min-rows 150000 \
         -min-new-rows 3000000 \
         -window-factor 2.0 \
         -check-wait-seconds 120 \
         -record-file "../datanum_checkpoint.json.txt" \
         "$@" \
         2>&1 | tee "$BASEDIR"/count_data.log &

    wait
)

#set +x

echo ""
echo ""

exit 0
}
