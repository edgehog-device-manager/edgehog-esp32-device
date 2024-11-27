from pytest_embedded import Dut

def test_edgehog_device(dut, redirect):
     dut.expect_unity_test_output()
     assert len(dut.testsuite.testcases) == 3
     assert dut.testsuite.attrs['failures'] == 0
     assert dut.testsuite.attrs['errors'] == 0
