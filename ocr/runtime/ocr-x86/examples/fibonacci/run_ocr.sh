#export OCR_NUM_THREADS=36

export OCR_NUM_THREADS=24
for cutoff in 20 21 22 23 24 25 26 27 28 29 30; do
./fibonacci.x86.ocr.tbb.static.exec -md phi -bind ../cholesky/guadalupe.bind 50 $cutoff
done
