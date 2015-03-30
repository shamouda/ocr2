rm default.cfg
~/xstg/xstack/ocr/scripts/Configs/config-generator.py --threads $2
OCR_CONFIG=default.cfg make benchmark $1 PROG=ocr/${1}.c
OCR_CONFIG=default.cfg make run $1
