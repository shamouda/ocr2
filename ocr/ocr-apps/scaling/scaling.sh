NUMRUNS=10

THREADS=(1 2 4 8 16)
LOGX=(0 1 2 3 4)
N=5
OUTFILE=$1

for i in ${THREADS[*]}
do
    rm -f temp.cfg; ../../scripts/Configs/config-generator.py --threads $i --output temp.cfg
    SUM=0
    for j in `seq 1 $NUMRUNS`
    do
        X=`OCR_CONFIG=${PWD}/temp.cfg make -f Makefile.x86-pthread-x86 run 25|grep Time|cut -d\  -f2`
        SUM=`echo ${SUM}+${X}|bc -l`
    done
    LOGY+=(`echo l\($SUM/${NUMRUNS}\)/l\(2\)|bc -l`)
done

#echo ${LOGX[*]}
#echo ${LOGY[*]}

#Compute the regression line

SXY=0
SX=0
SY=0
SX2=0

for i in ${LOGX[*]}
do
    SXY=`echo $SXY+${LOGX[$i]}*${LOGY[$i]}|bc -l`
    SX=`echo $SX+${LOGX[$i]}|bc -l`
    SY=`echo $SY+${LOGY[$i]}|bc -l`
    SX2=`echo $SX2+${LOGX[$i]}*${LOGX[$i]}|bc -l`
done

NUM=`echo $N*$SXY-$SX*$SY|bc -l`
DEN=`echo $N*$SX2-$SX*$SX|bc -l`
ANS=`echo $NUM/$DEN|bc -l`
rm -f temp.cfg

#Output CSV
echo "fib" >> $OUTFILE
echo $ANS >> $OUTFILE
