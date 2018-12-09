#
# Copyright (C) 2018 Every-fucking-one, except the Author
#
# This source code is licensed under the GLWTS Public License found in the
# LICENSE file in the root directory of this source tree.
#

.PHONY: clean all a2sdn

CXX ?= g++-5
CXXFLAGS = -std=c++11 -Wall -fdiagnostics-color=always -D'_LIBCPP_EXTERN_TEMPLATE(...)='

A2SDN_SRC = a2sdn.cc
A2SDN_PROG = a2sdn

SRCS = $(A2SDN_SRC) controller.cc switch.cc
PROGS = $(A2SDN_PROG)
OBJS = controller.o switch.o

# Compile all exectuables
all: $(PROGS)

a2sdn: $(A2SDN_SRC) $(OBJS)
	@$(CXX) $(CXXFLAGS) -o $(A2SDN_PROG) $(A2SDN_SRC) $(OBJS)

controller.o: controller.cc
	@$(CXX) $(CXXFLAGS) -c $^ -o $@

switch.o: switch.cc
	@$(CXX) $(CXXFLAGS) -c $^ -o $@

t1-fifo:
	@mkfifo -m 666 fifo-1-0 fifo-0-1

t2-fifo:
	@mkfifo -m 666 fifo-1-0 fifo-0-1 fifo-2-0 fifo-0-2 fifo-1-2 fifo-2-1

# Archive submission
tar:
	tar -cvf submit.tar $(SRCS) a2sdn.h switch.h controller.h Makefile README.pdf README.md

# Clean workspace
clean:
	@rm -rf $(PROGS) $(OBJS)
	@find . -type f -name 'fifo-*-*' -delete 2> /dev/null
