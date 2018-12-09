#
# Copyright (C) 2018 Every-fucking-one, except the Author
#
# This source code is licensed under the GLWTS Public License found in the
# LICENSE file in the root directory of this source tree.
#

.PHONY: clean all a1jobs a1mon

CXX ?= g++-5
CXXFLAGS = -std=c++11 -Wall -fdiagnostics-color=always -D'_LIBCPP_EXTERN_TEMPLATE(...)='

A1JOBS_SRC = a1jobs.cc
A1JOBS_PROG = a1jobs

A1MON_SRC = a1mon.cc
A1MON_PROG = a1mon

SRCS = $(A1JOBS_SRC) $(A1MON_SRC)
PROGS = $(A1JOBS_PROG) $(A1MON_PROG)

# Compile all exectuables
all: $(PROGS)

a1jobs: $(A1JOBS_SRC)
	@$(CXX) $(CXXFLAGS) -o $(A1JOBS_PROG) $(A1JOBS_SRC)

a1mon: $(A1MON_SRC)
	@$(CXX) $(CXXFLAGS) -o $(A1MON_PROG) $(A1MON_SRC)

# Archive submission
tar:
	tar -cvf submit.tar $(SRCS) a1mon.h a1jobs.h Makefile README.pdf README.md

# Clean workspace
clean:
	@rm -rf $(PROGS)
