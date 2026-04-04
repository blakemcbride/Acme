MAIN = main
TEX = xelatex
TEXFLAGS = -interaction=nonstopmode -halt-on-error

all: Acme-book.pdf

Acme-book.pdf: $(MAIN).tex ch*.tex app*.tex biblio.tex
	$(TEX) $(TEXFLAGS) $(MAIN).tex
	makeindex $(MAIN)
	$(TEX) $(TEXFLAGS) $(MAIN).tex
	$(TEX) $(TEXFLAGS) $(MAIN).tex
	mv $(MAIN).pdf Acme-book.pdf

draft:
	$(TEX) $(TEXFLAGS) $(MAIN).tex
	mv $(MAIN).pdf Acme-book.pdf

clean:
	rm -f *.aux *.log *.toc *.lof *.lot *.idx *.ilg *.ind *.out *.bbl *.blg

realclean: clean
	rm -f Acme-book.pdf

.PHONY: all draft clean realclean
