#! /usr/bin/python3

from jonchki import jonchkihere

import os
import subprocess
import sys

# --------------------------------------------------------------------------
# -
# - Configuration
# -

# Get the project folder. This is the folder of this script.
strCfg_projectFolder = os.path.dirname(os.path.realpath(__file__))

# This is the complete path to the testbench folder. The installation will be
# written there.
strCfg_workingFolder = os.path.join(
    strCfg_projectFolder,
    'targets'
)

# Where is the jonchkihere tool?
strCfg_jonchkiHerePath = os.path.join(
    strCfg_projectFolder,
    'jonchki'
)
# This is the Jonchki version to use.
strCfg_jonchkiVersion = '0.0.6.1'

# This is ther verbose level. It can be one of the strings 'debug', 'info',
# 'warning', 'error' or 'fatal'.
strCfg_jonchkiVerbose = 'debug'

# Look in this folder for Jonchki archives before downloading them.
strCfg_jonchkiLocalArchives = os.path.join(
    strCfg_projectFolder,
    'jonchki',
    'local_archives'
)
# The target folder for the jonchki installation. A subfolder named
# "jonchki-VERSION" will be created there. "VERSION" will be replaced with
# the version number from strCfg_jonchkiVersion.
strCfg_jonchkiInstallationFolder = os.path.join(
    strCfg_projectFolder,
    'targets'
)

strCfg_jonchkiLog = os.path.join(
    strCfg_workingFolder,
    'jonchki.log'
)
strCfg_jonchkiSystemConfiguration = os.path.join(
    strCfg_projectFolder,
    'jonchki',
    'jonchkisys.cfg'
)
strCfg_jonchkiProjectConfiguration = os.path.join(
    strCfg_projectFolder,
    'jonchki',
    'jonchkicfg.xml'
)
# Get the full path to the finalizer.
strCfg_jonchkiFinalizer = os.path.join(
    strCfg_projectFolder,
    'jonchki',
    'finalizer.lua'
)
strCfg_jonchkiDependencyLog = os.path.join(
    strCfg_projectFolder,
    'dependency-log.xml'
)
strCfg_jonchkiArtifactConfiguration = os.path.join(
    strCfg_projectFolder,
    'jonchki',
    'flash_app.xml'
)

# -
# --------------------------------------------------------------------------

# Create the folders if they do not exist yet.
astrFolders = [
    strCfg_workingFolder,
    os.path.join(strCfg_workingFolder, 'build_requirements')
]
for strPath in astrFolders:
    if os.path.exists(strPath) is not True:
        os.makedirs(strPath)

# Install jonchki.
strJonchki = jonchkihere.install(
    strCfg_jonchkiVersion,
    strCfg_jonchkiInstallationFolder,
    LOCAL_ARCHIVES=strCfg_jonchkiLocalArchives
)

# ---------------------------------------------------------------------------
#
# Get the build requirements.
#

# Define environment variables here or use None.
astrEnv = None

# Run Jonchki to install the build requirements.
astrCmd = [
    strJonchki,
    'install-dependencies',
    '--verbose', strCfg_jonchkiVerbose,
    '--syscfg', strCfg_jonchkiSystemConfiguration,
    '--prjcfg', strCfg_jonchkiProjectConfiguration,
    '--finalizer', strCfg_jonchkiFinalizer
]
astrCmd.append('--build-dependencies')
astrCmd.append(strCfg_jonchkiArtifactConfiguration)
strCwd = os.path.join(strCfg_workingFolder, 'build_requirements')
subprocess.check_call(' '.join(astrCmd), shell=True, cwd=strCwd, env=astrEnv)


# ---------------------------------------------------------------------------
#
# Build the firmware.
#
astrCmd = [
    sys.executable,
    'mbs/mbs'
]
strCwd = strCfg_projectFolder
subprocess.check_call(' '.join(astrCmd), shell=True, cwd=strCwd, env=astrEnv)
