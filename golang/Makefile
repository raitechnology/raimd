# raimd makefile
lsb_dist     := $(shell if [ -f /etc/os-release ] ; then \
                  grep '^NAME=' /etc/os-release | sed 's/.*=[\"]*//' | sed 's/[ \"].*//' ; \
                  elif [ -x /usr/bin/lsb_release ] ; then \
                  lsb_release -is ; else echo Linux ; fi)
lsb_dist_ver := $(shell if [ -f /etc/os-release ] ; then \
		  grep '^VERSION=' /etc/os-release | sed 's/.*=[\"]*//' | sed 's/[ \"].*//' ; \
                  elif [ -x /usr/bin/lsb_release ] ; then \
                  lsb_release -rs | sed 's/[.].*//' ; else uname -r | sed 's/[-].*//' ; fi)
uname_m      := $(shell uname -m)

short_dist_lc := $(patsubst CentOS,rh,$(patsubst RedHatEnterprise,rh,\
                   $(patsubst RedHat,rh,\
                     $(patsubst Fedora,fc,$(patsubst Ubuntu,ub,\
                       $(patsubst Debian,deb,$(patsubst SUSE,ss,$(lsb_dist))))))))
short_dist    := $(shell echo $(short_dist_lc) | tr a-z A-Z)

# this is where the targets are compiled
build_dir ?= $(short_dist)$(lsb_dist_ver)_$(uname_m)$(port_extra)

.EXPORT_ALL_VARIABLES:

# Build configuration
PROGRAM = test_msg_go
SOURCE = test_msg_go.go

# CGO configuration  
CGO_CFLAGS = -I$(PWD)/../include
CGO_LDFLAGS = -L$(PWD)/../$(build_dir)/lib64 -lraimd -Wl,-rpath,$(PWD)/../$(build_dir)/lib64

# Default target
all: $(PROGRAM)

# Build the program
$(PROGRAM): $(SOURCE)
	go build -o $(PROGRAM) $(SOURCE)

# Clean build artifacts
clean:
	rm -f $(PROGRAM)

.PHONY: all clean
