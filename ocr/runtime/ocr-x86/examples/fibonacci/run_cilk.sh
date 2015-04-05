# export CILK_NWORKERS=36
# taskset -c 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35 
# export CILK_NWORKERS=18
# taskset -c 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17 
#export CILK_NWORKERS=16
#taskset -c 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 
#export CILK_NWORKERS=8
#taskset -c 0,1,2,3,4,5,6,7 
#export CILK_NWORKERS=4
#taskset -c 0,1,2,3 
#export CILK_NWORKERS=2
#taskset -c 0,1 

export CILK_NWORKERS=24
for cutoff in 20 21 22 23 24 25 26 27 28 29 30; do
taskset -c 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23 ./fibonacci.cilk.exec 50 $cutoff
done
