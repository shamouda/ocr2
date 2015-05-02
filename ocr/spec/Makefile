SRC_FILES=Source/*.tex

all: ocr-0.99.pdf

ocr-0.99.pdf: $(SRC_FILES)
	TEXINPUTS=.:./ocr_doxygen: pdflatex ocr-0.99
	TEXINPUTS=.:./ocr_doxygen: bibtex ocr-0.99
	TEXINPUTS=.:./ocr_doxygen: pdflatex ocr-0.90
	TEXINPUTS=.:./ocr_doxygen: pdflatex ocr-0.99

.PHONY:clean
clean:
	-rm -f *.bbl *.blg *.brf *.aux *.idx *.imp *.log *.out *.toc ocr-0.99.pdf *~ \#*
