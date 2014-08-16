# Default program for "python -m PyInventor": starts scene graph editor
# Author: Thomas Moeller
#
# Copyright (C) the PyInventor contributors. All rights reserved.
# This file is part of PyInventor, distributed under the BSD 3-Clause
# License. For full terms see the included COPYING file.
#

import sys
from PyInventor.QtInventor.Editor import QSceneGraphEditor

if __name__ == '__main__':
    app = QSceneGraphEditor(sys.argv)
    sys.exit(app.exec_())
