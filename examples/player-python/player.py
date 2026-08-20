# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the feather-tk project.

import sys
import opentimelineio as otio
import ftkPy as ftk
import tlRenderPy as tl

import App

context = ftk.Context()
tl.ui.init(context)
app = App.App(context, sys.argv)
if app.hasCmdLineHelp:
    sys.exit(0)
app.run()
app = None

