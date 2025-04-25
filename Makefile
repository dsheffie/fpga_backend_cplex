CPLEX=/opt/ILOG/CPLEX
FLAGS=-m64 -I$(CPLEX)/cplex/include -I$(CPLEX)/concert/include -DIL_STD
LIBS = -L$(CPLEX)/cplex/lib/x86-64_sles10_4.1/static_pic -lilocplex -lcplex -L$(CPLEX)/concert/lib/x86-64_sles10_4.1/static_pic -lconcert -lm -pthread 

OBJS = schedPass.o cplexScheduler.o dataFlowAlgos.o allocateStorage.o util.o dataPathBuilder.o systemParam.o controlFSMBuilder.o generateVerilogPreamble.o schedulerBase.o

all: $(OBJS)
	g++ $(OBJS) -fPIC -shared -o schedPass.so $(LIBS)

%.o: %.cpp
	g++ -g -fPIC -c $< `llvm-config --cppflags` $(FLAGS)

clean: 
	rm -rf *.so $(OBJS)

rebuild: clean all
