# import common functions for netedit tests
import os
import sys

testRoot = os.path.join(os.environ.get('SUMO_HOME', '.'), 'tests')
neteditTestRoot = os.path.join(
    os.environ.get('TEXTTEST_HOME', testRoot), 'netedit')
sys.path.append(neteditTestRoot)
import neteditTestFunctions as netedit

# Open netedit
neteditProcess, match = netedit.setupAndStart(neteditTestRoot, False)

# apply zoom
netedit.zoomIn(match.getTarget().offset(325, 200), 10)

# go to additional mode
netedit.additionalMode()

# select E3
netedit.changeAdditional("e3Detector")

# create E3
netedit.leftClick(match, 250, 100)

# select entry detector
netedit.changeAdditional("detEntry")

# Create Entry detector
netedit.selectAdditionalChild(4, 0)
netedit.leftClick(match, 100, 250)

# change to move mode
netedit.moveMode()

# move Entry to right
netedit.moveElement(match, 15, 250, 150, 250)

# go to inspect mode
netedit.inspectMode()

# inspect Entry
netedit.leftClick(match, 200, 250)

# block additional
netedit.modifyBoolAttribute(2)

# change to move mode
netedit.moveMode()

# try to move Entry to right (must be blocked)
netedit.moveElement(match, 150, 250, 300, 250)

# go to inspect mode
netedit.inspectMode()

# inspect Entry
netedit.leftClick(match, 200, 250)

# unblock additional
netedit.modifyBoolAttribute(2)

# change to move mode
netedit.moveMode()

# move Entry to right (must be allowed)
netedit.moveElement(match, 150, 250, 300, 250)

# Check undos and redos
netedit.undo(match, 6)
netedit.redo(match, 6)

# save additionals
netedit.saveAdditionals()

# save newtork
netedit.saveNetwork()

# quit netedit
netedit.quit(neteditProcess, False, False)
