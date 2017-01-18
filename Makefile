# set up build vars
CXX       = g++
CFLAGS    = $(shell pkg-config fuse --cflags) -fno-strict-aliasing -ggdb \
            -Wall -MMD -msse2 -mfpmath=sse -march=core2 -mtune=core2 -O3 \
            -DFUSE_USE_VERSION=26 -std=gnu++0x
LDFLAGS   = $(shell pkg-config fuse --libs)

# specify paths
OBJDIR    = ./obj
FILENAMES = myfs
CFILES    = $(patsubst %, %.cpp, $(FILENAMES))
OFILES    = $(patsubst %, $(OBJDIR)/%.o, $(FILENAMES))
DFILES    = $(patsubst %, $(OBJDIR)/%.d, $(FILENAMES))
GOAL      = $(OBJDIR)/myfs

# build rules
.PHONY: all clean realclean info

all: info $(OBJDIR) $(GOAL)
	@echo $(GOAL) complete

# convince 'make' not to worry if a .d is missing
$(DEPS):

# to keep the build lines comprehensible, we hide the actual CXX invocations.
# Printing this information first helps if we need to recreate the invocation
# manually
info:
	@echo "Building with CFLAGS=${CFLAGS}"
	@echo "and LDFLAGS=${LDFLAGS}"

# /clean/ deletes everything from the obj directory
clean:
	@rm -f $(OFILES) $(GOAL)

# /realclean/ also kills the directory and the dependencies
realclean:
	@rm -rf $(OBJDIR)

# build the directory in which the .o files will go
$(OBJDIR):
	mkdir -p $@

# compilation rule for making .o files from a cpp
$(OBJDIR)/%.o: %.cpp
	@echo [${CXX}] $< "-->" $@
	@$(CXX) $(CFLAGS) -c $< -o $@

# compilation rule for making an executable from a .o
$(GOAL): $(OFILES)
	@echo [$(CXX)] "$< --> " $@
	@$(CXX) -o $@ $(OFILES) $(LDFLAGS)

-include $(DEPS)
