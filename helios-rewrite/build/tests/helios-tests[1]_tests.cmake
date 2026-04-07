add_test([=[Placeholder.CompilationWorks]=]  /home/ryutribal/programming/helios/helios-rewrite/build/bin/helios-tests [==[--gtest_filter=Placeholder.CompilationWorks]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Placeholder.CompilationWorks]=]  PROPERTIES DEF_SOURCE_LINE /home/ryutribal/programming/helios/helios-rewrite/tests/ecs/test_placeholder.cpp:3 WORKING_DIRECTORY /home/ryutribal/programming/helios/helios-rewrite/build/tests SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  helios-tests_TESTS Placeholder.CompilationWorks)
