INCLUDES = include
LIBS = -lboost_system-mt -lgrapejson -lboost_program_options -lev -lmsgpack -lcrypto++

testapp: worker.o main.o application.o function_handler.o
	g++ -o testapp main.o worker.o application.o function_handler.o $(LIBS)

worker.o: src/worker.cpp
	g++ -I $(INCLUDES) -std=c++0x -o worker.o -c src/worker.cpp
	
function_handler.o: src/function_handler.cpp
	g++ -I $(INCLUDES) -std=c++0x -o function_handler.o -c src/function_handler.cpp
	
application.o: src/application.cpp
	g++ -I $(INCLUDES) -std=c++0x -o application.o -c src/application.cpp

main.o: main.cpp
	g++ -I $(INCLUDES) -std=c++0x -o main.o -c main.cpp