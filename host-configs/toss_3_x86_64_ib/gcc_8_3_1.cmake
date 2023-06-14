set(ENABLE_MPI On CACHE BOOL "")
set(CMAKE_C_COMPILER "/usr/tce/packages/gcc/gcc-8.3.1/bin/gcc" CACHE PATH "")
set(CMAKE_CXX_COMPILER "/usr/tce/packages/gcc/gcc-8.3.1/bin/g++" CACHE PATH "")
set(CMAKE_Fortran_COMPILER "/usr/tce/packages/gcc/gcc-8.3.1/bin/gfortran" CACHE PATH "")
set(umpire_DIR "/usr/WS1/samrai/samrai-tpl/toss_3_x86_64_ib/2023_02_16_10_00_37/gcc-8.3.1/umpire-2022.03.1/lib/cmake/umpire" CACHE PATH "")
set(RAJA_DIR "/usr/WS1/samrai/samrai-tpl/toss_3_x86_64_ib/2023_02_16_10_00_37/gcc-8.3.1/raja-2022.03.0/lib/cmake/raja" CACHE PATH "")
set(camp_DIR "/usr/WS1/samrai/samrai-tpl/toss_3_x86_64_ib/2023_02_16_10_00_37/gcc-8.3.1/camp-2022.03.2/lib/cmake/camp" CACHE PATH "")
set(ENABLE_UMPIRE TRUE CACHE BOOL "")
set(ENABLE_RAJA TRUE CACHE BOOL "")
set(MIN_TEST_PROCS 2 CACHE INT "")

