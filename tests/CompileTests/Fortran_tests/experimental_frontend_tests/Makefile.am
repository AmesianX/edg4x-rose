include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs

# DQ (6/8/2013): Use command line option to alternatively test the experimental fortran frontend.
ROSE_FLAGS = -rose:verbose 0 -rose:experimental_fortran_frontend -I$(srcdir)

VALGRIND_OPTIONS = --tool=memcheck -v --num-callers=30 --leak-check=no --error-limit=no --show-reachable=yes --trace-children=yes
# VALGRIND = valgrind $(VALGRIND_OPTIONS)
VALGRIND =

TEST_F90_Objects = main_program_2008_01.f90

PASSING_TEST_Objects = $(TEST_F90_Objects)

# Go back and build the translator we are using to test Fortran (in case make check 
# is run directly in this directory before make has been run from the root of the 
# ROSE directory structure).  This does not appear to work!
../../../testTranslator:
	cd ../../..; $(MAKE) testTranslator

testTranslator: ../../../testTranslator

$(TEST_F90_Objects): ../../../testTranslator
	$(VALGRIND) ../../../testTranslator $(ROSE_FLAGS) -rose:f90 -c $(srcdir)/$(@:.o=.f90)
if ROSE_EXPERIMENTAL_OFP_ROSE_CONNECTION
	sglri -i $(srcdir)/junk.f90 -p $(top_builddir)/src/3rdPartyLibraries/experimental-fortran-parser/Fortran.tbl | implode-asfix | pp-aterm -o junk.f90.aterm
endif

junk.ast-info.pp: $(srcdir)/junk.f90 $(top_builddir)/src/3rdPartyLibraries/experimental-fortran-parser/Fortran.tbl
#	sglri -i $(srcdir)/junk.f90 -p $(top_builddir)/src/3rdPartyLibraries/experimental-fortran-parser/Fortran.tbl | addPosInfo -i junk.pt -p $(srcdir)/junk.f90 -m | implode-asfix | pp-aterm -o junk.ast-info.pp
	sglr -i $(srcdir)/junk.f90 -p $(top_builddir)/src/3rdPartyLibraries/experimental-fortran-parser/Fortran.tbl | addPosInfo -p $(srcdir)/junk.f90 -m | implode-asfix | pp-aterm -o junk.ast-info.pp

junk : $(srcdir)/junk.f90 $(top_builddir)/src/3rdPartyLibraries/experimental-fortran-parser/Fortran.tbl
	sglri -i $(srcdir)/junk.f90 -p $(top_builddir)/src/3rdPartyLibraries/experimental-fortran-parser/Fortran.tbl | implode-asfix | pp-aterm -o junk.f90.aterm


clean-local:
	rm -f rose_*.* *.aterm *.o

check-local:
	@echo "Tests for experimental Fortran frontend."
if ROSE_EXPERIMENTAL_OFP_ROSE_CONNECTION
	@$(MAKE) $(PASSING_TEST_Objects)
	@echo "*******************************************************************************************************************************"
	@echo "****** ROSE/tests/CompileTests/Fortran_tests/experimental_frontend_tests: make check rule complete (terminated normally) ******"
	@echo "*******************************************************************************************************************************"
else
	@echo "*******************************************************************************************"
	@echo "***** Skipping tests of experimental Fortran frontend (must be configured explicitly) *****"
	@echo "*******************************************************************************************"
endif

