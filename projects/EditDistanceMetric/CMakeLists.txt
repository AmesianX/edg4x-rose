
include_directories(${KDE4_INCLUDES} ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )


########### next target ###############

set(astDiff_SRCS
    astDiff.C
    sequenceGeneration.C
    treeIsomorphism.C
    sequenceAlignment.C
    heatMap.C
    attributeSupport.C
    treeEditDistance.C)

kde4_add_executable(astDiff ${astDiff_SRCS})

target_link_libraries(astDiff ${KDE4_KDECORE_LIBS})


########### install files ###############




#original Makefile.am contents follow:

#include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs
#
## "if" for DQ_DEVELOPER_TESTS (AutoMake conditional block)
#if DQ_DEVELOPER_TESTS 
## if USE_BINARY_ANALYSIS_TESTS
#
#INCLUDES = $(ROSE_INCLUDES) 
#
#noinst_PROGRAMS = astDiff
#
## shortestPathBoostExample
#
#astDiff_SOURCES = astDiff.C sequenceGeneration.C treeIsomorphism.C sequenceAlignment.C heatMap.C attributeSupport.C treeEditDistance.C
#
## Files not required:
## treeDiff.C
## labelTrace.C
#
#astDiff_LDADD   = $(ROSE_LIBS_WITH_PATH) $(ROSE_SEPARATE_LIBS) $(RT_LIBS)
#
#
## shortestPathBoostExample_SOURCES = shortestPathBoostExample.C
## shortestPathBoostExample_LDADD   = $(ROSE_LIBS_WITH_PATH) $(ROSE_SEPARATE_LIBS) $(RT_LIBS)
#
#TESTCODES_REQUIRED_TO_PASS = 
#
#TESTCODE_CURRENTLY_FAILING = 
#
## ******************************************************
##              Standard Makefile Content
## ******************************************************
#
#testProgram_1: $(srcdir)/testProgram.c
#	gcc -g -o testProgram_1 $(srcdir)/testProgram.c
#
#testProgram_2: $(srcdir)/testProgram.c
#	gcc -o testProgram_2 $(srcdir)/testProgram.c
#
## Test of astEquivalence on Binary Executable
#testBinaryEquivalence: astDiff testProgram_1 testProgram_2
#	./astDiff -rose:read_executable_file_format_only testProgram_1 testProgram_2
#
## Test of astEquivalence on Source Code
#testSourceEquivalence: astDiff $(srcdir)/testProgram.c $(srcdir)/testProgram_alt.c
#	cat $(srcdir)/testProgram.c $(srcdir)/testProgram_alt.c
#	./astDiff $(srcdir)/testProgram.c $(srcdir)/testProgram_alt.c
#
#testBoost: shortestPathBoostExample
#	./shortestPathBoostExample
#
## This demo generates the edit graph for two similar source code files
## and displays the generated visualization of the graph using zgrviewer.
#demoSourceEquivalence: testSourceEquivalence
#	cp output.dot outputSource.dot
#	zgrviewer outputSource.dot &
#
## This demo generates the edit graph for two similar binary files (one
## compiled with debug and the other without), and displays a pre-generated
## image of a visualization of the a quadrant of the whole graph (takes 
## 20+ minutes to layout in dot so the image has been pre-computed).
#demoBinaryEquivalence: testBinaryEquivalence
#	eog $(srcdir)/astEquivalenceGraphForBinaryCode3.png &
#
## ******************************************************
##              Standard Makefile Content
## ******************************************************
#
## Automake's testing mechanism (which defines the "make check" rule) requires passing tests.
#TESTCODES = \
#$(TESTCODES_REQUIRED_TO_PASS)
#
## QMTest allows both passing and failing tests.
#ALL_TESTCODES = \
#$(TESTCODES_REQUIRED_TO_PASS) \
#$(TESTCODE_CURRENTLY_FAILING)
#
## File option to accumulate performance information about the compilation
#PERFORMANCE_REPORT_OPTION = -rose:compilationPerformanceFile $(top_builddir)/DISASSEMBLER_ROSE_PERFORMANCE_DATA.csv
#
#ROSE_FLAGS = -rose:verbose 2 $(PERFORMANCE_REPORT_OPTION)
#
#VALGRIND_OPTIONS = --tool=memcheck -v --num-callers=30 --leak-check=no --error-limit=no --show-reachable=yes
## VALGRIND = /usr/apps/valgrind/new/bin/valgrind $(VALGRIND_OPTIONS)
#VALGRIND =
#
#PASSING_TEST_Objects = ${TESTCODES}
#TEST_Objects = ${ALL_TESTCODES}
#
#$(TEST_Objects): binaryEquivalence
##	$(VALGRIND) ./binaryReader $(ROSE_FLAGS) -c $(srcdir)/$(@:.o=.c)
#	$(VALGRIND) ./binaryReader $(srcdir)/$@ $(srcdir)/$@_alt
#
#copyFiles:
#	cp $(srcdir)/*.h $(top_srcdir)/tests/CompilerTests/astEquivalence_tests
#	cp $(srcdir)/*.c $(top_srcdir)/tests/CompilerTests/astEquivalence_tests
#
#check-local:
#	@echo "Dan Quinlan's development tests for Binary Disassembly."
##  Run this test explicitly since it has to be run using a specific rule and can't be lumped with the rest
##	These C programs must be called externally to the test codes in the "TESTCODES" make variable
#	@$(MAKE) $(PASSING_TEST_Objects)
#	@echo "***********************************************************************************************************************"
#	@echo "****** ROSE/developersScratchSpace/Dan/astEquivalence_tests: make check rule complete (terminated normally)        ******"
#	@echo "***********************************************************************************************************************"
## else
## check-local:
## 	@echo "Dan Quinlan's development tests for Binary astEquivalence."
## #  Run this test explicitly since it has to be run using a specific rule and can't be lumped with the rest
## #	These C programs must be called externally to the test codes in the "TESTCODES" make variable
## 	@$(MAKE) $(PASSING_TEST_Objects)
## 	@echo "***********************************************************************************************************************"
## 	@echo "****** ROSE/developersScratchSpace/Dan/astEquivalence_tests: skipped due to --disable-binary-analysis-tests    ***"
## 	@echo "***********************************************************************************************************************"
## endif
#
#else
#check-local:
#	@echo "\nDan Quinlan's Binary astEquivalence development tests NOT run.  To run these reconfigure with:"
#	@echo "*********************************************************************************************"
#	@echo "************** make check not run here: configure --enable-dq-developer-tests ***************"
#	@echo "*********************************************************************************************"
#endif
#
## DQ (1/21/2009): Used as part of demos (since zgrviewer takes too long to visualize the associated dot file (25 minutes).
#EXTRA_DIST = astEquivalenceGraphForBinaryCode3.png
#
#clean-local:
#	rm -f *.o rose_*.[s] rose_*.? *.dot *.png *.pdf *~ *.ps *.out *.new *.dump sqlite-database-name.* *.stderr *.stdout *.exe testProgram rose_performance_report_lockfile.lock
#	rm -f testProgram_1  testProgram_2
#	rm -rf QMTest
#
