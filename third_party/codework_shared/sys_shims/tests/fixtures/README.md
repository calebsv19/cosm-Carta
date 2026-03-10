# sys_shims Regression Fixtures

This directory stores fixture metadata used by `sys_shims_conformance_test`.

- `regression_cases_v1.csv`: canonical regression case manifest.

The conformance runner verifies that required case IDs are present in the manifest.
That guard prevents accidental loss of tracked regression coverage.
