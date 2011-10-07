EXE=xdb

all:
	g++ -o $(EXE) main.cpp

clean:
	rm $(EXE)
