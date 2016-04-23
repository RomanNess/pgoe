#!/bin/bash


CCG=./CubeCallGraphTool
# output is dumped here
S_OUT=spec-output-stats
# cubex profiles of spec benchmarks
S_IN=spec-centos

rm -rf $S_OUT
mkdir $S_OUT

PARAMS="--mangled"

while [[ $# > 0 ]]
do
key="$1"

case $key in
    -s|--samples)
    PARAMS+=" --samples $2"
	shift # past argument
    ;;
    -i|--ignore-sampling)
    PARAMS+=" --ignore-sampling"
    ;;
    -t|--tiny)
    PARAMS+=" --tiny"
    ;;
    *)
    # unknown option
    ;;
esac
shift # past argument or value
done

#$CCG $S_IN/403.gcc.scorep.cubex              -h 105 -r 40.3  $PARAMS 2>&1 | tee $S_OUT/403.gcc.clang.scorep.log
$CCG $S_IN/429.mcf.clang.scorep.cubex        -h 105 -r 236.2 $PARAMS 2>&1 | tee $S_OUT/429.mcf.clang.scorep.log
$CCG $S_IN/433.milc.clang.scorep.cubex       -h 105 -r 421.7 $PARAMS 2>&1 | tee $S_OUT/433.milc.clang.scorep.log
$CCG $S_IN/444.namd.clang.scorep.cubex       -h 105 -r 425.7 $PARAMS 2>&1 | tee $S_OUT/444.namd.clang.scorep.log
#CCG $S_IN/447.dealII.clang.scorep.cubex     -h 105 -r 26.5  $PARAMS 2>&1 | tee $S_OUT/447.dealII.clang.scorep.log
$CCG $S_IN/450.soplex.clang.scorep.cubex     -h 105 -r 102.9 $PARAMS 2>&1 | tee $S_OUT/450.soplex.clang.scorep.log
$CCG $S_IN/456.hmmer.clang.scorep.cubex      -h 105 -r 166.6 $PARAMS 2>&1 | tee $S_OUT/456.hmmer.clang.scorep.log
$CCG $S_IN/458.sjeng.clang.scorep.cubex      -h 105 -r 332.9 $PARAMS 2>&1 | tee $S_OUT/458.sjeng.clang.scorep.log
$CCG $S_IN/462.libquantum.clang.scorep.cubex -h 105 -r 509.9 $PARAMS 2>&1 | tee $S_OUT/462.libquantum.clang.scorep.log
$CCG $S_IN/464.h264ref.clang.scorep.cubex    -h 105 -r 402.8 $PARAMS 2>&1 | tee $S_OUT/464.h264ref.clang.scorep.log
#$CCG $S_IN/470.lbm.clang.scorep.cubex        -h 105 -r 71.2  $PARAMS 2>&1 | tee $S_OUT/470.lbm.clang.scorep.log
$CCG $S_IN/473.astar.clang.scorep.cubex      -h 105 -r 365.7 $PARAMS 2>&1 | tee $S_OUT/473.astar.clang.scorep.log
$CCG $S_IN/482.sphinx3.clang.scorep.cubex    -h 105 -r 157.1 $PARAMS 2>&1 | tee $S_OUT/482.sphinx3.clang.scorep.log
$CCG $S_IN/453.povray.gcc.scorep.cubex       -h 105 -r 535.2 $PARAMS 2>&1 | tee $S_OUT/453.povray.gcc.scorep.log
	
#	mv Instrument-callgraph.dot $SPEC_OUTPUT/$FILE.dot

#	generate the dot
#	dottopng.sh Instrument-callgraph.dot spec-png/$FILE.png

