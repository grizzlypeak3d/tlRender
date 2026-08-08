#!/bin/sh

# PYBIND11_MODULE puts a function body inside a macro, so gcov reports the end
# line of a module init function as the line the macro starts on, and lcov 2
# treats that disagreement as fatal and abandons the whole capture. Filtering
# is not an alternative: tlRenderPy is part of what is being measured, and the
# capture fails before any of the removals below run. feather-tk ignores it the
# same way for the same reason.
lcov -c -b . -d . -o coverage.info --ignore-errors mismatch
lcov -r coverage.info '*/usr/*' -o coverage_filtered.info
lcov -r coverage_filtered.info '*/install/*' -o coverage_filtered.info
lcov -r coverage_filtered.info '*/tests/*' -o coverage_filtered.info
# deps is feather-tk, which measures its own coverage in its own CI, and the
# vendored loaders it carries below that. Neither is tlRender's to report.
lcov -r coverage_filtered.info '*/deps/*' -o coverage_filtered.info
lcov --list coverage_filtered.info
