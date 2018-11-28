# Copyright (C) 2018 Michael (Zichun) Lin
# All rights reserved - Do Not Redistribute

.PHONY: clean all a4tasks

CXX ?= g++-5
CXXFLAGS = -std=c++17 -pthread -Wall -fdiagnostics-color=always -D'_LIBCPP_EXTERN_TEMPLATE(...)='

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
