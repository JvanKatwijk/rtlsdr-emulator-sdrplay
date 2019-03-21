
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

-------------------------------------------------------------------------------
Use under Windows
------------------------------------------------------------------------------

For use under Windows, a precompiled version of the dll
is available.
This repository contains a zipped folder with - as example - the
rtl433.exe program (Copyright Benjamin Larsson (merbanan on github)),
together with the fake librtlsdr.dll and other
required libraries to run the rtl_433.exe program.

-----------------------------------------------------------------------------
Creating - and using - the library under Linux
-----------------------------------------------------------------------------

It will be no surprise that the usage under Linux is more or less
the same as it is under Windows: just replace the librtlsdr.so by the
newly generated fake one.

(For simplicity reasons, the include files for both libraries are included)
Execute

	gcc -O2 -fPIC -shared  -I . -o librtlsdr.o rtlsdr-bridge.c -lmirsdrapi-rsp

and replace the librtlsdr.so* files in - most likely - /usr/local/lib by
this new librtlsdr.so.

If you want to see some of the parameters used for the SDRplay, add

	-D__DEBUG__

to the command line.

The SDRplay will then be able to behave (to a large extend)
as an RTLDSDR device if an RTLSDR device is asked for.

------------------------------------------------------------------------------
Issues
-------------------------------------------------------------------------------

There are quite some differences between the two API's, so there is not
a 100% match. However, since in most cases only a subset of the rtlsdr
API is used, it turns out that programs like acarsdec, dump1090, rtl_433,
aiswatcher, dump978 run with the emulator.

The major "issue" - still being worked on - is the translation of
the gain setting of the DAB sticks.
It turns out - playing around with e.g. acarsdec that the result
is pretty depending on the selection of the right
lnaState and Gain Reduction for the SDRplay


The current "approach' is - for the RSPII - for lower frequencies (< 420M)
table driven, it works reasonably wel with the acarsdec software.

---------------------------------------------------------------------------
Creating a DLL for use under Windows
---------------------------------------------------------------------------

For creating a DLL the mingw32 environment is used. 

Under Windows, assuming you have msys2 (with mingw32) installed
(with gcc compiler), 
execute

	i686-w64-mingw32-gcc -O2 -fPIC -shared -I . -o rtlsdr.dll rtlsdr-bridge.c mir_sdr_api.dll.a

The command will generate a dll, "rtlsdr.dll" that can be used instead of
the original one.
A suggestion is to copy the original rtlsdr.dll to rtlsdr.dll-rtldevice

Note that for using the mingw32 compilers, one does need the ".dll.a" file,
which for reasons of convenience is part of the distribution

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

