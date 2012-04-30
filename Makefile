BASE_PATH	  = ../..
CXXFLAGS      := -g -I/usr/include/ -Wall -DSTAND_ALONE -I$(BASE_PATH)/libxml/include -I$(BASE_PATH)/kernel/bsp/gemppc/iolite/2.6.14/ -I$(BASE_PATH)
export LDFLAGS       := -lm -ldl -lrt
#export XC = /opt/crosstool/powerpc-linux/bin/powerpc-405-linux-gnu-
export CXX = $(XC)g++
export LD  = $(CXX)



.PHONY: common.hpp #the makefile will require these headers unless we make them phony targets
	
#shared objects that do not correspond to a particular binary, header files are assumed to have the same base name
SHARED		  = common.o ../base/file.o ../base/hash.o

INCLUDE		  = $(BASE_PATH)/include/messaging.hpp $(BASE_PATH)/include/file.hpp #headers that replace the phony headers

#names of the binarys to produce, also the names of the corresponding .cpp and .hpp files
TARGETS       = messaging_test

.PHONY: all
all:            $(TARGETS)
	@echo "done"

clean:      
	@rm -f $(TARGETS) $(TARGETS:=.o) $(SHARED) *.o *.core

#todo use template if/when there are a lot of targets
messaging_test: $(SHARED) $(TARGETS:=.cpp) $(INCLUDE)
	$(LD) $@.cpp $(CXXFLAGS) $(LDFLAGS)  $(SHARED) -o $@	

$(TARGETS:=.hpp):

$(TARGETS:=.cpp):

%.o:

