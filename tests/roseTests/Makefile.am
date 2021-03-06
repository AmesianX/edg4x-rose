include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs

# SUBDIRS =  \
#      PHPTests \
#      astFileIOTests \
#      astInliningTests \
#      astInterfaceTests \
#      astMergeTests \
#      astOutliningTests \
#      astPerformanceTests \
#      astProcessingTests \
#      astQueryTests \
#      astRewriteTests \
#      astSymbolTableTests \
#      astTokenStreamTests \
#      binaryTests \
#      loopProcessingTests \
#      ompLoweringTests \
#      programAnalysisTests \
#      programTransformationTests \
#      roseHPCToolkitTests \
#      utilTests \
#      fileLocation_tests \
#      graph_tests \
#      mergeTraversal_tests

# DQ (4/17/2010): These directories are now organized based on what sets of languages 
# are enabled in ROSE (including binary analysis).
SUBDIRS =
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
   SUBDIRS += astMergeTests astPerformanceTests \
              astProcessingTests astQueryTests astRewriteTests astSymbolTableTests astTokenStreamTests \
              programTransformationTests \
              graph_tests mergeTraversal_tests \
	      astLValueTests abstractMemoryObjectTests
if !ROSE_USE_INSURE
 SUBDIRS += roseHPCToolkitTests 
endif

# TOO (2/16/2011): Errors with Thrifty (GCC 3.4.4) backend compiler. We can gradually
# enable these tests at a later stage if necessary.
if !USING_GCC_3_4_4_BACKEND_COMPILER
# TOO (2/17/11): Errors with Tensilica's Xtensa compilers. 
if !USING_XTENSA_BACKEND_COMPILER
#  SUBDIRS += astInterfaceTests astInliningTests ompLoweringTests astFileIOTests programAnalysisTests astOutliningTests loopProcessingTests fileLocation_tests
if !ROSE_USE_INSURE
   SUBDIRS += astInterfaceTests astInliningTests ompLoweringTests
endif
   SUBDIRS += astFileIOTests 
if !ROSE_USE_INSURE
   SUBDIRS += programAnalysisTests astOutliningTests loopProcessingTests fileLocation_tests
endif 
endif
endif
endif

if ROSE_BUILD_PHP_LANGUAGE_SUPPORT
   SUBDIRS += PHPTests
endif

if !ROSE_USE_INSURE
# DQ (4/23/2011): At least one of the files (process_cmdline.o) in this directory does not seem 
# to compile with Insure++ (it hangs for Hudson, but I can't reproduce the problem locally).
# This was not a problem the second time that Insure++ compiled ROSE.

# DQ (8/23/2013): This is a part of SUBDIRS above and is redundant here.
# DQ (9/2/2011): utilTests has C++ specific tests.
# SUBDIRS += roseHPCToolkitTests utilTests
# SUBDIRS += roseHPCToolkitTests

# DQ (9/2/2011): Only run these tests if C or C++ is supported.
#if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
   SUBDIRS += utilTests
#endif
endif

# Binary tests are a current problem for Insure++.
if !ROSE_USE_INSURE
if ROSE_BUILD_BINARY_ANALYSIS_SUPPORT
   SUBDIRS += binaryTests
endif
endif
