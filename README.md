
rtlsdr emulator: Running rtlsdr-based software with an SDRplay device

--------------------------------------------------------------------------

There is no doubt that the (various) SDRplay RSP devices are technically
superior to the common RTLSDR "dab" sticks. That being true, there is
also no doubt that there is much more software written - and made available -
for the RTLSDR sticks than for the SDRplay devices.

After adding SDRplay support to some rtlsdr-based software packages, i.e.
dump1090 and acarsdec, it was felt that rewriting each piece of 
rtlsdr based software for SDRplay support was not an ideal approach.

---------------------------------------------------------------------------

The approach taken here is to bridge the gap between the API of the SDRplay
devices - that API is well documented - and the osmocom API of the RTLSDR
devices.

A library is created that implements a - reasonable - part of the 
osmocom defined API in terms of the SDRplay RSP API. 
Installing this library creates the possibility of running - unmodified -
programs that believe they are bound to "the" rtlsdr dynamic library,
but actually run with the SDRplay device.


-------------------------------------------------------------------------------
Use under Windows
------------------------------------------------------------------------------

For use under Windows, a precompiled version of the dll
is available.
The usage is simple: just replace the rtlsdr.dll by the one in the 
repository. Note however, that more dll's are required.
The respository contains a zipped folder with - as example - the
rtl433.exe program (Copyright Benjamin Larsson (merbanan on github)),
together with the fake librtlsdr.dll and other
required libraries.

----------------------------------------------------------------------------
Use under Linux
----------------------------------------------------------------------------

The usage is - obviously - quite similar to the usage under Windows.
Just create a version of the "fake" library, as indicated here
under the assumption that the "sdrapi" library is installed.

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

----------------------------------------------------------------------------

There are quite some differences between the two API's, so there is not
a 100% match. However, since in most cases only a subset of the rtlsdr
API is used, it turns out that programs like acarsdec, dump1090, rtl_433,
aiswatcher, dump978 run with the emulator.



---------------------------------------------------------------------------
Creating a DLL for use under Windows
---------------------------------------------------------------------------

For creating a DLL the mingw32 environment is used. 

Under Windows, assuming you have msys2 installed (with gcc compiler), 

	gcc -O2 -fPIC -shared -I . -o rtlsdr.dll rtlsdr-bridge.c mir_sdr_api.dll.a

Under Linux, assuming mingw32 is installed

	i686-w64-mingw32-gcc -O2 -fPIC -shared -I . -o rtlsdr.dll rtlsdr-bridge.c mir_sdr_api.dll.a

The command will generate a dll, "rtlsdr.dll" that can be used instead of
the original one. A suggestion is to copy the original rtlsdr.dll to rtlsdr,dll-rtldevice

Note that for using the mingw32 compilers, one does need the ".dll.a" file, which for reasons of convenience is
part of the distribution

------------------------------------------------------------------------------
Copyrights
------------------------------------------------------------------------------

This software:	
  * Copyright (C) 2018 Jan van Katwijk, Lazy Chair Computing

SDRplay:
  * Copyright (C) SDRplay inc.

rtl-sdr:
 * Copyright (C) 2012-2013 by Steve Markgraf <steve@steve-m.de>
 * Copyright (C) 2012 by Dimitri Stolnikov <horiz0n@gmx.net>

rtl433.exe program:
 * Copyright Benjamin Larsson (merbanan on github)

