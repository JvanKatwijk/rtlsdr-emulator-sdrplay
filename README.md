
--------------------------------------------------------------------------
rtlsdr emulator: Running rtlsdr-based software with an SDRplay device
--------------------------------------------------------------------------

There is no doubt that the (various) SDRplay RSP devices are technically
superior to the common RTLSDR "dab" sticks. That being said, there is
also no doubt that there is much more software written - and made available -
for the RTLSDR sticks than for the SDRplay devices.

After adding SDRplay support to some rtlsdr-based software packages, i.e.
dump1090 and acarsdec, it was felt that rewriting each piece of 
rtlsdr based software for SDRplay support was hardly an ideal approach.

---------------------------------------------------------------------------

Therefore an other approach was taken: create a library that shows itself as
an RTLSDR library, and internally translates calls to appropriate calls
to the SDRplay libary,  bridging the gap between the API of the SDRplay
devices. 

Of course it is required to have the dll (or shared lib) for the
SDRplay device installed,
after all, that is the one doing the work. The fake rtlsdr library
(librtlsdr.so, or rtlsdr.dll, or librtlsdr.dll as it should
be named for some windows executables) is merely
a translator from commands to the rtlsdr api to
commands to the SDRplay api.

-----------------------------------------------------------------------------
Use under Windows (or how to run sdr# with the SDRplay)
-----------------------------------------------------------------------------

Mapping the gain setting as used on an RTLSDR 2832 based stick to
the more elaborate settings for SDRplay was found to be unsatisfactory.
A decent balance has to be found between selecting the gain reduction
in the LNA (by choosing an lna state) and the if Gain reduction.

Therefore, for Windows an alternative solution is being developed,
under Windows, the gain setting to the stick - as often done in the
program - is cut short and a small dialog box shows that allows
the precise setting for the lna state and the Gainreduction

![rtlsdr emulator](/rtlsdr-emulator-windows.png?raw=true)

The picture shows - top left - a small dialog box "owned" by the
rtlsdr.dll file that allows a precise setting of the lna state and
the gain reduction. The small checkbox in the dialog window is
for controlling the setting of the AGC.

 On the right one sees the original widget for the
rtlsdr software, note that gain setting in this latter widget has no effect,
gain setting is controlled by the SDRplay gain setting widget.

------------------------------------------------------------------------------
Installing under Windows
------------------------------------------------------------------------------

Although it is quite well possible to build a dll file, the repository contains
one, together with the other dll's needed to run the emulator.

As an example, to run sdr# with the SDRplay, all dll's from the example folder
which is part of the repository, are copied into the directory (folder)  of the SDR#,
with the result as shown above.

------------------------------------------------------------------------------
Use under Linux 
-------------------------------------------------------------------------------

For Linux the gain setting as set by the user is translated in a setting for
the LNA and one for the if gain reduction.
The "algorithm" is that first the gain is translated into a gain reduction and
the LNA is made "responsible" for one third of this,
the other part to be delivered by the if gain reduction.

Therefore, under Linux, using the emulator is completely transparant.

![rtlsdr emulator](/rtlsdr-emulator-linux.png?raw=true)

-------------------------------------------------------------------------------
Installing under Linux
-------------------------------------------------------------------------------

For Linux one - obviously - can create a shared library (see below).
However, the repository contains a precompiled *librtlsdr.so* file, that
can be placed in e.g. /usr/local/lib

-------------------------------------------------------------------------------
Building a library file
------------------------------------------------------------------------------

For use under Windows, a precompiled version of the dll
is available. Best is to look at the example program, that contains
all dll's necessary to run the emulated dll driver.

The  repository contains two makefiles, one for Linux and one
for (cross) compilation for Windows.
Note that the sources contain some compiler directives #ifdef __MINGW32__
to separate the code sections for the windows version from the sections
for the Linux version.

The "librtlsdr.so" file in the repository was made under Fedora 
The "rtlsdr.dll" file in the respository was made under Fedora, using
the Mingw64 32bits resource and C compilers.

------------------------------------------------------------------------------
Issues
-------------------------------------------------------------------------------

There are quite some differences between the two API's, so there is not
a 100% match. However, since in most cases only a subset of the rtlsdr
API is used, it turns out that programs like acarsdec, dump1090, rtl_433,
aiswatcher, dump978 run pretty well with the emulator.

An issue is that the range of samplerate between the devices differs,
while RTLSDR based sticks support a range of 960 KHz .. 2.5 MHz, the SDRplay
supports 2 Mhz and up to 8 (or 10) Mhz.
Samplesrates for the RTLSDR stick between 1 and 2 MHz are handled by the
emulerator using the double of this rate and decimating with a factor of 2.

The Windows version is still slightly experimental - for me (being a Unix/Linux
person for the last 45 years) it is the very first program using a windows API directly -
and it is most likely that some changes will be applied.

------------------------------------------------------------------------------
Copyrights
------------------------------------------------------------------------------

This software:	
  * Copyright (C) 2018, 2019 Jan van Katwijk, Lazy Chair Computing

SDRplay:
  * Copyright (C) SDRplay inc.

rtl-sdr:
 * Copyright (C) 2012-2013 by Steve Markgraf <steve@steve-m.de>
 * Copyright (C) 2012 by Dimitri Stolnikov <horiz0n@gmx.net>

rtl433.exe program:
 * Copyright Benjamin Larsson (merbanan on github)

