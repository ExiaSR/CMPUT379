#
# Copyright (C) 2018 Every-fucking-one, except the Author
#
# This source code is licensed under the GLWTS Public License found in the
# LICENSE file in the root directory of this source tree.
#

.PHONY: clean all a2sdn

CXX ?= g++-5
CXXFLAGS = -std=c++11 -pthread -Wall -fdiagnostics-color=always -D'_LIBCPP_EXTERN_TEMPLATE(...)='

A3SDN_SRC = a3sdn.cc
A3SDN_PROG = a3sdn

SRCS = $(A3SDN_SRC) controller.cc switch.cc
PROGS = $(A3SDN_PROG)
OBJS = controller.o switch.o

# Compile all exectuables
all: $(PROGS)

a3sdn: $(A3SDN_SRC) $(OBJS) a3sdn.h
	@$(CXX) $(CXXFLAGS) -o $(A3SDN_PROG) $(A3SDN_SRC) $(OBJS)

controller.o: controller.cc
	@$(CXX) $(CXXFLAGS) -c $^ -o $@

switch.o: switch.cc
	@$(CXX) $(CXXFLAGS) -c $^ -o $@

# Archive submission
tar:
	tar -cvf submit.tar.gz $(SRCS) a3sdn.h switch.h controller.h Makefile README.pdf README.md

# Clean workspace
clean:
	@rm -rf $(PROGS) $(OBJS)
	@find . -type f -name 'fifo-*-*' -delete 2> /dev/null
