
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

-----------------------------------------------------------------------------------------

Use is simple

-----------------------------------------------------------------------------------------

The usage is simple: just replace the rtlsdr.dll (in case of windows), or the librtlsdr.so
(and librtlsdr.so,x) (for linux and run "sudo ldconfig" ) with the newly generated one and
the SDRplay will behave (to a large extend) as an RTLDSDR device.

----------------------------------------------------------------------------

There are quite some differences between the two API's, so there is not
a 100% match. However, since in most cases only a subset of the rtlsdr
API is used, it turns out that programs like acarsdec, dump1090, rtl_433,
aiswatcher, dump978 run with the emulator.


-----------------------------------------------------------------------------

Creating the library under Linux

-----------------------------------------------------------------------------


For simplicity reasons, the include files for both libraries are included.
Assuming the library for the SDRplay RSP device is installed, execute

	gcc -O2 -fPIC -shared  -I . -o librtlsdr.o rtlsdr-bridge.c -lmiradrapi-rsp

and replace the librtlsdr.so* files in - most likely - /usr/local/lib by
this new librtlsdr.so

---------------------------------------------------------------------------

Creating a DLL

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

------------------------------------------------------------------------------------------------------------------------

Copyrights

-------------------------------------------------------------------------------------------------------------------------

This software:	
  * Copyright (C) 2018 Jan van Katwijk, Lazy Chair Computing

SDRplay:
  * Copyright (C) SDRplay inc.

rtl-sdr:
 * Copyright (C) 2012-2013 by Steve Markgraf <steve@steve-m.de>
 * Copyright (C) 2012 by Dimitri Stolnikov <horiz0n@gmx.net>

