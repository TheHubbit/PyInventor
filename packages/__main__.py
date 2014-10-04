# Default program for "python -m PyInventor": starts scene graph editor
# Author: Thomas Moeller
#
# Copyright (C) the PyInventor contributors. All rights reserved.
# This file is part of PyInventor, distributed under the BSD 3-Clause
# License. For full terms see the included COPYING file.
#

"""
PyInventor main entry point.

When invoking the module as an executable (python -m PyInventor)
the scene graph editor application will be launched.
"""

import sys
from PyInventor.QtInventor import QSceneGraphEditor

if __name__ == '__main__':
    app = QSceneGraphEditor(sys.argv)
    app.show()
    sys.exit(app.exec_())
