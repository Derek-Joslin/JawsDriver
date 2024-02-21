JAWS Braille Driver SDK

The BrailleDriverSdk.chm  file documents the IBrailleDriver interface
that all drivers must implement.

The SampleBrailleDriver subdirectory contains a Visual Studio 2022 C++
project representing a skeleton Braille driver. Look for comments that
begin with "ToDo:" in both SampleBrailleDriver.cpp and
SampleBrailleDriver.h for places where you'll need to addd code
specific to your driver.  You should also update
SampleBrailleDriver.rc with manufacturer and version info for your
driver.

You'll probably also want to rename everything to have more
appropriate nnames.

The sample Braille driver  includes an ARM64 configuration. In order to build for ARM64, the ARM64 compiler must be installed as part of
Visual Studio 2022. There are two things that must be done:

Under Workloads/Windows Desktop, in the optional components
section, make sure that
MSVC ARM64 build tools is checked.

Under Individual Compponents, make sure that ATL for ARM64 is
checked. Not doing this will result in the ARM64 compiler failing to
find atls.lib.

This package also contains supporting materials as follows:
JAWS Split Braille 3rd Party Braille vendors.docx
In JAWS 2024, Vispero introduced the split Braille feature.  This document describes how to get the most our of this feature with your display and supporting configuration files.

BrailleTestProcedure.xlsx
This document provides a list of tests that should be run with your driver to verify functionality. Once your driver passes these tests, it is ready to be sent to Vispero for Vispero validation and certification.

ThirdPartyBrailleInstallation.doc
This document explains requirements to create a stand-alone installer to install your driver and supporting files for use with any versions of JAWS installed on the system.

