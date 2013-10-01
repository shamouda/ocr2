OCR_INPUT_HOME=/scratch/st6/Cholesky
PROG=cholesky
SUFFIX=.exe
CFLAGS= -g -O3
OCR_FLAGS=-L${OCR_INSTALL}/lib -L${HPCTOOLKIT_PATH}/lib/hpctoolkit -I${OCR_INSTALL}/include -I${HPCTOOLKIT_PATH}/include -locr -lm -lhpctoolkit
OCR_INPUT=${OCR_INPUT_HOME}

all: compile run
all-test: compile run verify

MATRIX_SIZE=6000
TILE_SIZE=80

INPUT_FILE=m_${MATRIX_SIZE}.in
OUTPUT_FILE=cholesky_out_${MATRIX_SIZE}.txt

compile:
	gcc $(CFLAGS) $(OCR_FLAGS) -I. $(PROG).c -o $(PROG)$(SUFFIX)

compile_pp:
	g++ $(CFLAGS) $(OCR_FLAGS) -I. $(PROG).cpp -o $(PROG)$(SUFFIX)

run:
	./$(PROG)$(SUFFIX) ${MATRIX_SIZE} ${TILE_SIZE} ${OCR_INPUT}/${INPUT_FILE}

clean:
	-rm -Rf *.o $(PROG)$(SUFFIX) cholesky.out

verify:
	diff -b ${OCR_INPUT}/$(OUTPUT_FILE) cholesky.out
