SRC_FILES=Source/*.tex

all: ocr-1.1.0.pdf

ocr-1.1.0.pdf: $(SRC_FILES)
	TEXINPUTS=.:./ocr_doxygen: pdflatex ocr-1.1.0
	TEXINPUTS=.:./ocr_doxygen: bibtex ocr-1.1.0
	TEXINPUTS=.:./ocr_doxygen: pdflatex ocr-1.1.0
	TEXINPUTS=.:./ocr_doxygen: pdflatex ocr-1.1.0

.PHONY:clean
clean:
	-rm -f *.bbl *.blg *.brf *.aux *.idx *.imp *.log *.out *.toc ocr-1.1.0.pdf *~ \#*
