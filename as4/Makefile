#
# Copyright (C) 2018 Every-fucking-one, except the Author
#
# This source code is licensed under the GLWTS Public License found in the
# LICENSE file in the root directory of this source tree.
#

.PHONY: clean all a4tasks

CXX ?= g++-5
CXXFLAGS = -std=c++11 -pthread -Wall -fdiagnostics-color=always -D'_LIBCPP_EXTERN_TEMPLATE(...)='

A4_SRC = main.cc
A4_PROG = a4tasks

SRCS = $(A4_SRC)
PROGS = $(A4_PROG)
OBJS = utils.o

# Compile all exectuables
all: $(PROGS)

a4tasks: $(A4_SRC) $(OBJS)
	@$(CXX) $(CXXFLAGS) -o $(A4_PROG) $(A4_SRC) $(OBJS)

utils.o: utils.cc
	@$(CXX) $(CXXFLAGS) -c $^ -o $@

# Archive submission
tar:
	tar -cvf submit.tar.gz *.cc *.h Makefile README.pdf README.md

# Clean workspace
clean:
	@rm -rf $(PROGS) $(OBJS)
