if PYTHON_SWIG_ENABLED
TESTSCRIPTS = \
        DeletingSwigIteratorObtainedFromPublicListRegTest.py \
        UsingTheNextMethodOnASwigIteratorRegTest.py \
        JobSupervisorTest.py EndpointContainerTest.py \
        EndpointTest.py ServiceEndpointRetrieverTest.py \
        TargetInformationRetrieverTest.py \
        MappingOf_time_t_and_uint32_t_CTypesToPythonRegressionTest.py
endif

AUXFILES = testutils.py
