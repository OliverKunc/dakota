#!/usr/bin/env python
import unittest
import h5py
import h5py_console_extract as hce

_TEST_NAME = "mixed_sampling"

class Moments(unittest.TestCase):

    def setUp(self):
        # This method will be called once for each test, so cache the moments
        try:
            self._moments
        except AttributeError:
            self._moments = hce.extract_moments()
            self._cis = hce.extract_moment_confidence_intervals()

    def test_structure(self):
        # Verify the structure (execution id, descriptors)
        console_moments = self._moments
        expected_num_execs = len(console_moments)
        expected_descriptors = set(console_moments[0].keys())
        expected_executions = set("execution:%d" % (i+1,) for i in range(len(console_moments)))
        with h5py.File(_TEST_NAME + ".h5","r") as h:
            ## Verify the presence of all the execution data
            # execution:N
            self.assertEqual(expected_num_execs, len(list(h["/methods/aleatory/results/"].keys())))
            for n in h["/methods/aleatory/results/"].keys():
                self.assertTrue(n in expected_executions)
            # descriptors
            hdf5_descriptors = set(h["/methods/aleatory/results/execution:1/moments/"].keys())
            self.assertEqual(expected_descriptors, hdf5_descriptors)

    def test_moments(self):
        # Extract the moments from Dakota console output and compare
        # them to the hdf5 output. Both the moments themselves and
        # the dimension scales are compared.
        console_moments = self._moments
        expected_scale_label = 'moments'
        expected_scale = ['mean', 'std_deviation', 'skewness', 'kurtosis']
        expected_descriptors = set(console_moments[0].keys())
        expected_num_execs = len(console_moments)

        with h5py.File(_TEST_NAME + ".h5","r") as h:
            # moments and scales
            for i in range(expected_num_execs):
                for r in expected_descriptors:
                    hdf5_moments = h["/methods/aleatory/results/execution:%d/moments/%s" % (i+1, r)]
                    for j in range(4):
                        self.assertAlmostEqual(console_moments[i][r][j], hdf5_moments[j])
                    scale_label = list(hdf5_moments.dims[0].keys())[0]
                    self.assertEqual(expected_scale_label, scale_label)
                    for es, s in zip(expected_scale, hdf5_moments.dims[0][0]):
                        self.assertEqual(es,s)

    def test_moment_confidence_intervals(self):
        console_cis = self._cis
        expected_scale_labels = ['bounds', 'moments']
        expected_scales = [["lower", "upper"], ['mean', 'std_deviation']]
        expected_descriptors = set(console_cis[0].keys())
        expected_num_execs = len(console_cis)

        with h5py.File(_TEST_NAME + ".h5","r") as h:
            # CIs and scales
            for i, ci in enumerate(console_cis):
                hdf_cis = h["/methods/aleatory/results/execution:%d/moment_confidence_intervals/" % (i+1,)]
                for r in expected_descriptors:
                    self.assertEqual(expected_scale_labels[0], list(hdf_cis[r].dims[0].keys())[0])
                    self.assertEqual(expected_scale_labels[1], list(hdf_cis[r].dims[1].keys())[0])
                    for j in range(2):
                        for k in range(2):
                            self.assertAlmostEqual(ci[r][j][k], hdf_cis[r][j,k])
                    for j in range(2):
                        self.assertEqual(expected_scales[0][j], hdf_cis[r].dims[0][0][j])
                        self.assertEqual(expected_scales[1][j], hdf_cis[r].dims[1][0][j])

class PDFs(unittest.TestCase):
    def setUp(self):
        try:
            self._pdfs
        except AttributeError:
            self._pdfs = hce.extract_pdfs()

    def test_pdfs(self):
        console_pdfs = self._pdfs
        expected_scale_labels = set(('lower_bounds','upper_bounds'))
        expected_descriptors = set(console_pdfs[0].keys())
        expected_num_execs = len(console_pdfs)

        with h5py.File(_TEST_NAME + ".h5","r") as h:
            # pdfs and scales
            for i in range(expected_num_execs):
                for r in expected_descriptors:
                    hdf5_pdf = h["/methods/aleatory/results/execution:%d/probability_density/%s" % (i+1, r)]
                    console_pdf = console_pdfs[i][r]
                    # same number of bins
                    self.assertEqual(len(console_pdf), len(hdf5_pdf))
                    # labels
                    self.assertEqual(expected_scale_labels, set(hdf5_pdf.dims[0].keys()))
                    for j in range(len(hdf5_pdf)):
                        # Lower bound
                        self.assertAlmostEqual(console_pdf[j][0], hdf5_pdf.dims[0][0][j])
                        # Upper bound
                        self.assertAlmostEqual(console_pdf[j][1], hdf5_pdf.dims[0][1][j])
                        # Density
                        self.assertAlmostEqual(console_pdf[j][2], hdf5_pdf[j])

class LevelMappings(unittest.TestCase):
    def setUp(self):
        try:
            self._mappings
        except AttributeError:
            self._mappings = hce.extract_level_mappings()

    def test_level_mappings(self):
        console_mappings = self._mappings
        expected_descriptors = set(console_mappings[0].keys())
        expected_num_execs = len(console_mappings)

        # This test is tricky because it is not clear from the table of level mappings in the
        # table of console output what was specified by the user and what Dakota calculated.
        # Nevertheless, we know that:
        # 1. Dakota writes mappings for any user-specified response_levels first
        # 2. For the user-specified response_levels, Dakota will calculate exactly one of:
        #    probability, reliability, or generalized reliability
        # 3. After results for any response_levels, are written, Dakota reports results for
        #    user specified probability_levels, reliability_levels, and then generalized_
        #    reliability_levels, in that order.
        #
        # So, the testing strategy is, for each execution and response:
        # 1. Verify that the total number of levels (rows in the table) from the console equals 
        #    the number in the hdf5 file.
        # 2. Verify that the first entries in the table match the hdf5 response_levels. This is
        #    tricky because computed results may be reported in any of the remaining three columns,
        #    depending on what the user requested using the 'compute' keyword.
        # 3. Verify that the following rows match any probability, reliability, and gen. rel. results
        #    from the hdf5 file. These are easier because we know which columns to find the data and
        #    scales in.
                
        with h5py.File(_TEST_NAME + ".h5","r") as h:
            for i in range(expected_num_execs):
                for r in expected_descriptors:
                    # Create a set of the results present in the HDF5 file for convenience
                    has_result = set()
                    for result_type in ('response_levels', 'probability_levels', 
                            'reliability_levels', 'gen_reliability_levels'):
                        try:
                            h["/methods/aleatory/results/execution:%d/%s/%s" % (i+1, result_type, r)]
                            has_result.add(result_type)
                        except (KeyError, ValueError):
                            pass
                    # Verify the dimension scale label of the datasets
                    for label in has_result:
                        self.assertEqual(label,
                            h["/methods/aleatory/results/execution:%d/%s/%s" % (i+1, label, r)].dims[0].keys()[0])

                    # compare the total number of results in the console and hdf5 file
                    num_console_rows = len(console_mappings[i][r])
                    num_hdf5_rows = 0
                    for result_type in has_result:
                        num_hdf5_rows += len(h["/methods/aleatory/results/execution:%d/%s/%s" % (i+1, result_type, r)])
                    self.assertEqual(num_console_rows, num_hdf5_rows)
                    # Verify any response_levels
                    num_response_levels = 0
                    if 'response_levels' in has_result:
                        # Determine the 'compute'd column for the response_levels
                        for j, c in enumerate(console_mappings[i][r][0][1:]):
                            if c:
                                console_ri = j+1
                                break
                        # Loop over the response_levels and verify the user request and computed values
                        response_ds = h["/methods/aleatory/results/execution:%d/response_levels/%s" % (i+1, r)]
                        num_response_levels = len(response_ds)
                        for j in range(num_response_levels):
                            self.assertAlmostEqual(console_mappings[i][r][j][console_ri], response_ds[j])
                            self.assertEqual(console_mappings[i][r][j][0], response_ds.dims[0][0][j])
                        # Remove response_levels from the results because it has been processed
                        has_result.remove('response_levels')
                    # Verify any prob., rel., and gen. rel. levels
                    begin = num_response_levels
                    for j, result_type in enumerate(('probability_levels', 'reliability_levels', 'gen_reliability_levels')):
                        if result_type in has_result:
                            response_ds =  h["/methods/aleatory/results/execution:%d/%s/%s" % (i+1, result_type, r)]
                            num_hdf5_rows = len(response_ds)
                            end = begin+num_hdf5_rows
                            for cr, hr, s in zip(console_mappings[i][r][begin:end], response_ds, response_ds.dims[0][0]):
                                # Results
                                self.assertAlmostEqual(cr[0], hr)
                                # Scales
                                self.assertEqual(cr[j+1], s)
                            begin += num_hdf5_rows

if __name__ == '__main__':
    hce.run_dakota("dakota_hdf5_" + _TEST_NAME + ".in")
    unittest.main()

