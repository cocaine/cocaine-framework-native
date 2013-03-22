INCLUDES = include
LIBS = -lboost_system-mt -lgrapejson -lboost_program_options -lev -lmsgpack -luuid -lcrypto++

testapp: worker.o main.o logger.o application.o factories.o
	g++ -o testapp main.o worker.o application.o factories.o logger.o $(LIBS)

worker.o: src/worker.cpp
	g++ -I $(INCLUDES) -std=c++0x -o worker.o -c src/worker.cpp
	
factories.o: src/factories.cpp
	g++ -I $(INCLUDES) -std=c++0x -o factories.o -c src/factories.cpp
	
application.o: src/application.cpp
	g++ -I $(INCLUDES) -std=c++0x -o application.o -c src/application.cpp
	
logger.o: src/logger.cpp
	g++ -I $(INCLUDES) -std=c++0x -o logger.o -c src/logger.cpp

main.o: main.cpp
	g++ -I $(INCLUDES) -std=c++0x -o main.o -c main.cpp